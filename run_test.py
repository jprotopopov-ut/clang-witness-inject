#!/usr/bin/env python3
import sys
import pathlib
import argparse
import dataclasses
import json
import subprocess
import shutil
import shlex
import logging
import enum
from typing import Optional, List, Dict, Iterable, Any
from skip_invalid_assertions import ErroneousSegmentRemoval

SCRIPT_DIR=pathlib.Path(__file__).parent.resolve()
BASE_VARS = {
    'ROOT': str(SCRIPT_DIR)
}

@dataclasses.dataclass
class TestConfig:
    test_filepath: pathlib.Path
    cc_cmd: str
    cc_cflags: List[str]
    cc_ldflags: List[str]
    assert_cflags: List[str]
    goblint_cmd: str
    witness_inject_cmd: str
    clang_format_cmd: str
    sarif_cflags: List[str]
    assert_fn: str
    workdir_path: pathlib.Path
    goblint_conf_filepath: pathlib.Path
    run_command: str

@dataclasses.dataclass
class TestTaskConfig:
    test_filepath: pathlib.Path
    cc_cmd: str
    goblint_cmd: str
    witness_inject_cmd: str
    clang_format_cmd: str
    workdir_path: pathlib.Path
    test_conf_filepath: Optional[pathlib.Path]
    generic_conf_filepath: pathlib.Path

    def get_merged_conf(self) -> TestConfig:
        with open(self.generic_conf_filepath) as generic_conf_file:
            conf = json.load(generic_conf_file)

        if self.test_conf_filepath is not None:
            with open(self.test_conf_filepath) as test_conf_file:
                conf = {
                    **conf,
                    **json.load(test_conf_file)
                }

        return TestConfig(
            test_filepath=self.test_filepath,
            cc_cmd=self.cc_cmd,
            cc_cflags=conf['cc_cflags'],
            cc_ldflags=conf['cc_ldflags'],
            assert_cflags=conf['assert_cflags'],
            goblint_cmd=self.goblint_cmd,
            witness_inject_cmd=self.witness_inject_cmd,
            clang_format_cmd=self.clang_format_cmd,
            assert_fn=conf['assert_fn'],
            sarif_cflags=conf['sarif_cflags'],
            workdir_path=self.workdir_path,
            goblint_conf_filepath=conf['goblint_conf'],
            run_command=conf['run_cmd']
        )
    
def substitute_vars(string: str, vars: Dict[str, str]):
    for key, value in vars.items():
        string = string.replace(f'%{key}%', value)
    return string

def substitute_vars_multi(strings: Iterable[str], vars: Dict[str, str]):
    for string in strings:
        for key, value in vars.items():
            string = string.replace(f'%{key}%', value)
        yield string

def run_goblint(logger: logging.Logger, *, goblint_cmd: str, goblint_conf_filepath: pathlib.Path, witness_filepath: pathlib.Path, injected_filepath: pathlib.Path, workdir_path: pathlib.Path):
    goblint_argv = [
        goblint_cmd,
        '--conf',
        substitute_vars(goblint_conf_filepath, BASE_VARS),
        '--enable', 'witness.yaml.enabled',
        '--set', 'witness.yaml.path', str(witness_filepath),
        str(injected_filepath)
    ]
    logger.info('%s', shlex.join(goblint_argv))
    subprocess.check_call(
        executable=goblint_cmd,
        args=goblint_argv,
        shell=False,
        stdin=subprocess.DEVNULL,
        cwd=str(workdir_path)
    )

def inject_witness(logger: logging.Logger, *, witness_inject_cmd: str, cc_cflags: Iterable[str], assert_fn: str, witness_filepath: pathlib.Path, injected_filepath: pathlib.Path):
    witness_inject_argv = [
        witness_inject_cmd,
        *substitute_vars_multi(cc_cflags, BASE_VARS),
        '--witness-yaml', str(witness_filepath),
        '--assert-fn', assert_fn,
        str(injected_filepath)
    ]
    logger.info('%s', shlex.join(witness_inject_argv))
    subprocess.check_call(
        executable=witness_inject_cmd,
        args=witness_inject_argv,
        stdin=subprocess.DEVNULL,
        shell=False
    )

def clang_format(logger: logging.Logger, *, clang_format_cmd: str, filepath: pathlib.Path):
    clang_fmt_argv = [
        clang_format_cmd,
        '-i', str(filepath)
    ]
    logger.info('%s', shlex.join(clang_fmt_argv))
    subprocess.check_call(
        executable=clang_format_cmd,
        args=clang_fmt_argv,
        stdin=subprocess.DEVNULL,
        shell=False
    )

def cc_generate_sarif(logger: logging.Logger, *, cc_cmd: str, cc_cflags: Iterable[str], assert_cflags: Iterable[str], sarif_cflags: Iterable[str], input_filepath: pathlib.Path, sarif_filepath: pathlib.Path) -> Any:
    cc_argv = [
        cc_cmd,
        *substitute_vars_multi(cc_cflags, BASE_VARS),
        *substitute_vars_multi(assert_cflags, BASE_VARS),
        *substitute_vars_multi(sarif_cflags, BASE_VARS),
        str(input_filepath)
    ]
    logger.info('%s', shlex.join(cc_argv))
    sarif_proc = subprocess.Popen(
        executable=cc_cmd,
        args=cc_argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=False
    )
    sarif_output = b''
    while sarif_proc.poll() is None:
        _, sarif_stderr = sarif_proc.communicate()
        if sarif_stderr is not None:
            sarif_output += sarif_stderr
    sarif = json.loads(sarif_output.decode().splitlines()[1])
    with open(sarif_filepath, 'w') as sarif_file:
        json.dump(sarif, sarif_file, indent=2)
    return sarif

def cc_compile(logger: logging.Logger, *, cc_cmd: str, cc_cflags: Iterable[str], assert_cflags: Iterable[str], exe_filepath: pathlib.Path, input_filepath: pathlib.Path, cc_ldflags: Iterable[str]):
    cc_argv = [
        cc_cmd,
        *substitute_vars_multi(cc_cflags, BASE_VARS),
        *substitute_vars_multi(assert_cflags, BASE_VARS),
        '-o', str(exe_filepath),
        str(input_filepath),
        *substitute_vars_multi(cc_ldflags, BASE_VARS)
    ]
    logger.info('%s', shlex.join(cc_argv))
    subprocess.check_call(
        executable=cc_cmd,
        args=cc_argv
    )

def run_exe(logger: logging.Logger, *, run_command: str, exe_filepath: pathlib.Path):
    run_cmd = substitute_vars(run_command, {
        **BASE_VARS,
        'EXE': str(exe_filepath)
    })
    logger.info('%s', run_cmd)
    subprocess.check_call(
        run_cmd,
        shell=True
    )

class TestStage(enum.Enum):
    GoblintAnalyze = 'goblint'
    WitnessInject = 'inject'
    AssertCleanup = 'cleanup'
    Run = 'run'
    All = 'all'

    def is_stage(self, stage: 'TestStage') -> bool:
        return self == stage or self == TestStage.All

def run_test(logger: logging.Logger, stage: TestStage, config: TestConfig):
    injected_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('.injected.c')
    witness_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('.witness.yml')
    sarif_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('.sarif.json')
    injected_clean_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('.injected.clean.c')
    exe_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('')

    if stage.is_stage(TestStage.GoblintAnalyze):
        shutil.copy(config.test_filepath, injected_filepath)
        run_goblint(logger,
            goblint_cmd=config.goblint_cmd,
            goblint_conf_filepath=config.goblint_conf_filepath,
            witness_filepath=witness_filepath,
            injected_filepath=injected_filepath,
            workdir_path=config.workdir_path)

    if stage.is_stage(TestStage.WitnessInject):
        inject_witness(logger,
            witness_inject_cmd=config.witness_inject_cmd,
            cc_cflags=config.cc_cflags,
            assert_fn=config.assert_fn,
            witness_filepath=witness_filepath,
            injected_filepath=injected_filepath)
        
        clang_format(logger, clang_format_cmd=config.clang_format_cmd, filepath=injected_filepath)

    if stage.is_stage(TestStage.AssertCleanup):
        sarif = cc_generate_sarif(logger,
            cc_cmd=config.cc_cmd,
            cc_cflags=config.cc_cflags,
            assert_cflags=config.assert_cflags,
            sarif_cflags=config.sarif_cflags,
            input_filepath=injected_filepath,
            sarif_filepath=sarif_filepath)

        segment_removal = ErroneousSegmentRemoval(assert_fn=config.assert_fn)
        segment_removal.load_sarif(sarif)
        with open(injected_clean_filepath, 'w') as injected_clean_file:
            segment_removal.process_file(injected_filepath, injected_clean_file)

    if stage.is_stage(TestStage.Run):
        cc_compile(logger,
            cc_cmd=config.cc_cmd,
            cc_cflags=config.cc_cflags,
            assert_cflags=config.assert_cflags,
            exe_filepath=exe_filepath,
            input_filepath=injected_clean_filepath,
            cc_ldflags=config.cc_ldflags)

        run_exe(logger,
            run_command=config.run_command,
            exe_filepath=exe_filepath)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog=sys.argv[0], description='Witness Injection test runner')
    parser.add_argument('--generic-conf', type=str, required=True, help='Generic configuration file path')
    parser.add_argument('--goblint', type=str, required=True, help='Goblint')
    parser.add_argument('--cc', type=str, required=True, help='C compiler')
    parser.add_argument('--witness-inject', type=str, required=True, help='Witness injector')
    parser.add_argument('--clang-format', type=str, required=True, help='Clang format tool')
    parser.add_argument('--workdir', type=str, required=True, help='Working directory')
    parser.add_argument('--stage', type=str, choices=[stage.value for stage in TestStage], default=TestStage.All.value, help='Test stage')
    parser.add_argument('test_filepath', help='Test .c file path')

    logger = logging.Logger(pathlib.Path(__file__).name)
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(logging.Formatter('%(asctime)s — %(levelname)s — %(name)s — %(message)s'))
    logger.addHandler(handler)

    args = parser.parse_args(sys.argv[1:])
    test_filepath = pathlib.Path(args.test_filepath).resolve()
    test_conf_filepath = test_filepath.with_suffix('.json')
    if not test_conf_filepath.exists():
        test_conf_filepath = None
    generic_conf_filepath = pathlib.Path(args.generic_conf)
    workdir_path = pathlib.Path(args.workdir)
    workdir_path.mkdir(exist_ok=True)
    workdir_path = workdir_path.resolve()

    conf = TestTaskConfig(
        test_filepath=test_filepath,
        cc_cmd=args.cc,
        goblint_cmd=args.goblint,
        witness_inject_cmd=args.witness_inject,
        clang_format_cmd=args.clang_format,
        workdir_path=workdir_path,
        test_conf_filepath=test_conf_filepath,
        generic_conf_filepath=generic_conf_filepath
    ).get_merged_conf()
    logger.info('Starting the test with config %s', conf)

    run_test(logger, TestStage(args.stage), conf)
