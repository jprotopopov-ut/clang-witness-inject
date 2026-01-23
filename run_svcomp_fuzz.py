#!/usr/bin/env python3
import sys
import time
import pathlib
import argparse
import tempfile
import shutil
import zipfile
import logging
import aflpp_ctrl

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog=sys.argv[0], description='AFL++ driver for SV-COMP benchmarks')
    parser.add_argument('--aflpp', type=str, required=True, help='AFL++ installation directory')
    parser.add_argument('--state', type=str, required=False, help='State archive')
    parser.add_argument('--resume', default=False, action='store_true', help='Output directory')
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

        logger.info('Workdir %s', work_dir_path)

        if args.resume and args.state:
            logger.info('Loading state from %s', args.state)
            if args.program:
                logger.warning('Resuming fuzzing: %s is ignored in favor from archived version from %s', args.program, args.state)
            with zipfile.ZipFile(args.state, 'r') as archive:
                archive.extractall(work_dir_path)
        else:
            if not args.program:
                logger.critical('Missing program command line argument')
                sys.exit(-1)
            input_dir_path.mkdir()
            output_dir_path.mkdir()
            misc_dir_path.mkdir()
            shutil.copy(args.program, program_filepath)

        try:
            with open(input_dir_path / 'input1', 'wb') as sample_input:
                input_seed = b'\0' * 8 if not args.input_seed else args.input_seed.encode()
                logger.info('Using input seed %s', input_seed)
                sample_input.write(input_seed)
            
            config = aflpp_ctrl.AFLppFuzzConfig(
                aflpp_dir=pathlib.Path(args.aflpp),
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