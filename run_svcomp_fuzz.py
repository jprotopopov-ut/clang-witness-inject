#!/usr/bin/env python3
import sys
import os
import time
import pathlib
import argparse
import tempfile
import shutil
import shlex
import subprocess
import zipfile
import logging
import yaml
import aflpp_ctrl
import run_test
import strip_malformed_asserts

SCRIPT_DIR = pathlib.Path(__file__).parent.resolve()
FUZZ_HARNESS_DIR = SCRIPT_DIR / 'fuzz_harness'
EXAMPLES_DIR = SCRIPT_DIR / 'examples'

NULL_LOGGER = logging.Logger('null')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog=sys.argv[0], description='AFL++ driver for SV-COMP benchmarks')
    parser.add_argument('--aflpp', type=str, required=True, help='AFL++ installation directory')
    parser.add_argument('--state', type=str, required=False, help='State archive')
    parser.add_argument('--resume', default=False, action='store_true', help='Output directory')
    parser.add_argument('--compile', default=False, action='store_true', help='Compile benchmark program')
    parser.add_argument('--compile-cflags', type=str, default='', help='Compilation CFLAGS')
    parser.add_argument('--compile-ldflags', type=str, default='', help='Compilation LDFLAGS')
    parser.add_argument('--witness-inject-tool', type=str, default=str(SCRIPT_DIR / 'witness_inject'), help='Witness injection tool')
    parser.add_argument('--witness-yaml', type=str, required=False, help='Witness yaml file')
    parser.add_argument('--clang-cc', type=str, default='clang', help='Clang compiler')
    parser.add_argument('--clang-format', type=str, default='clang-format', help='Clang format tool')
    parser.add_argument('--quiet', default=False, action='store_true', help='Suppress normal AFL++ output')
    parser.add_argument('--input-seed', type=str, required=False, help='Initial input seed for fuzzing')
    parser.add_argument('--harness-timeout', type=int, default=5000, help='Fuzz harness timeout in microseconds')
    parser.add_argument('--timeout', type=int, default=0, help='Fuzz session timeout in seconds (0 is unlimited)')
    parser.add_argument('program', type=str, nargs='?', help='Benchmark program')

    logger = logging.Logger(pathlib.Path(__file__).name)
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(logging.Formatter('%(asctime)s — %(levelname)s — %(name)s — %(message)s'))
    logger.addHandler(handler)

    args = parser.parse_args(sys.argv[1:])

    found_crash = False
    with tempfile.TemporaryDirectory(delete=True) as work_dir:
        work_dir_path = pathlib.Path(work_dir)
        input_dir_path = work_dir_path / 'input'
        output_dir_path = work_dir_path / 'output'
        misc_dir_path = work_dir_path / 'misc'
        program_filepath = misc_dir_path / 'program'

        aflpp_dir = pathlib.Path(args.aflpp).resolve()

        logger.info('Workdir %s', work_dir_path)

        if args.resume and args.state:
            logger.info('Loading state from %s', args.state)
            if args.program:
                logger.warning('Resuming fuzzing: %s is ignored in favor from archived version from %s', args.program, args.state)
            with zipfile.ZipFile(args.state, 'r') as archive:
                archive.extractall(work_dir_path)
            os.chmod(program_filepath, 500)
        else:
            if not args.program:
                logger.critical('Missing program command line argument')
                sys.exit(-1)
            input_dir_path.mkdir()
            output_dir_path.mkdir()
            misc_dir_path.mkdir()

            if args.compile:
                program_source = args.program
                if args.witness_yaml:
                    injected_source = misc_dir_path / 'program.injected.c'
                    injected_clean_source = misc_dir_path / 'program.injected.clean.c'
                    witness_yaml = misc_dir_path / 'witness.yaml'
                    sarif_json = misc_dir_path / 'errors.json'

                    with open(args.witness_yaml) as input_yaml:
                        witness_yaml_content = list(yaml.safe_load(input_yaml))
                    for doc in witness_yaml_content:
                        if doc.get('entry_type') == 'invariant_set' and 'content' in doc:
                            for entry in doc['content']:
                                if 'invariant' in entry and 'location' in entry['invariant'] and 'file_name' in entry['invariant']['location']:
                                    entry['invariant']['location']['file_name'] = str(injected_source)
                    with open(witness_yaml, 'w') as output_yaml:
                        yaml.dump(witness_yaml_content, output_yaml)

                    shutil.copy(program_source, injected_source)
                    run_test.inject_witness(NULL_LOGGER,
                        witness_inject_cmd=args.witness_inject_tool,
                        cc_cflags=shlex.split(args.compile_cflags),
                        assert_fn='__WITNESS_ASSERT',
                        witness_filepath=witness_yaml,
                        injected_filepath=injected_source)
                    run_test.clang_format(NULL_LOGGER,
                        clang_format_cmd=args.clang_format,
                        filepath=injected_source)
                    
                    eraser = strip_malformed_asserts.MalformedAssertEraseDriver(
                        cc_cmd=args.clang_cc,
                        sarif_cflags=[
                            '-w', '-fsyntax-only', '-fdiagnostics-format=sarif', '-Wno-sarif-format-unstable', '-ferror-limit=0'
                        ]
                    )
                    with open(injected_clean_source, 'w') as out:
                        eraser.process_file(injected_source, out, cflags=[
                            *shlex.split(args.compile_cflags),
                            '-include', str(EXAMPLES_DIR / 'assert.h')
                        ], assert_fn='__WITNESS_ASSERT')
                    program_source = injected_clean_source
                logger.info('Compiling %s', program_source)
                subprocess.check_call([
                    aflpp_dir / 'bin' / 'afl-clang-lto',
                    '-w', '-fPIC', '-DFUZZ_HARNESS_RAND_STDIN=1',
                    '-include', str(FUZZ_HARNESS_DIR / 'fuzz_harness.h'),
                    '-include', str(EXAMPLES_DIR / 'assert.h'),
                    *shlex.split(args.compile_cflags),
                    program_source,
                    str(FUZZ_HARNESS_DIR / 'fuzz_harness.c'),
                    str(FUZZ_HARNESS_DIR / 'fuzz_harness.s'),
                    '-o', str(program_filepath),
                    *shlex.split(args.compile_ldflags),
                    '-Wl,-e,__fuzz_harness_start'
                ])
            else:
                shutil.copy(args.program, program_filepath)

        try:
            with open(input_dir_path / 'input1', 'wb') as sample_input:
                input_seed = b'\0' * 8 if not args.input_seed else args.input_seed.encode()
                logger.info('Using input seed %s', input_seed)
                sample_input.write(input_seed)
            
            config = aflpp_ctrl.AFLppFuzzConfig(
                aflpp_dir=aflpp_dir,
                skip_cpufreq=True,
                quiet=args.quiet
            )
            task = aflpp_ctrl.AFLppFuzzTask(
                executable=program_filepath,
                executable_args=list(),
                executable_env={
                    'FUZZ_HARNESS_RC': '0',
                    'FUZZ_HARNESS_TIMEOUT': str(args.harness_timeout)
                },
                input_dir=input_dir_path
            )
            with aflpp_ctrl.AFLppFuzzCtrl(out_dir=output_dir_path, config=config) as fuzz:
                if args.resume:
                    fuzz.resume(task=task)
                    logger.info('Resumed fuzzing')
                else:
                    fuzz.start(task=task)
                    logger.info('Started fuzzing')
                begin = time.time()

                try:
                    time.sleep(max(0.5, args.harness_timeout * 100 / 1e6))
                    while (args.timeout == 0 or time.time() - begin < args.timeout) and fuzz.is_running and not fuzz.has_crashes:
                        time.sleep(1)
                    if fuzz.failed:
                        logger.error('AFL++ fuzz process has failed')

                    found_crash = fuzz.has_crashes
                    logger.info('Found %s crash(-es), terminating', fuzz.crash_count)
                except KeyboardInterrupt:
                    logger.warning('Received keyboard interrupt, exiting')
                finally:
                    fuzz.stop(timeout=1)
        finally:
            if args.state:
                logger.info('Saving state to %s', args.state)
                shutil.make_archive(pathlib.Path(args.state).with_suffix(''), 'zip', work_dir_path)

    if found_crash:
        sys.exit(-1)