#!/usr/bin/env python3
import sys
import pathlib
import argparse
import dataclasses
import json
import subprocess
import shutil
from typing import Optional, List, Dict, Iterable
from skip_invalid_assertions import ErroneousSegmentRemoval

SCRIPT_DIR=pathlib.Path(__file__).parent.resolve()

@dataclasses.dataclass
class TestConfig:
    test_filepath: pathlib.Path
    cc_cmd: str
    cc_cflags: List[str]
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

def run_test(config: TestConfig):
    injected_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('.injected.c')
    witness_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('.witness.yml')
    sarif_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('.sarif.json')
    injected_clean_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('.injected.clean.c')
    exe_filepath = (config.workdir_path / config.test_filepath.name).with_suffix('')

    shutil.copy(config.test_filepath, injected_filepath)
    base_vars = {
        'ROOT': str(SCRIPT_DIR)
    }
    subprocess.check_call(
        executable=config.goblint_cmd,
        args=[
            config.goblint_cmd,
            '--conf',
            substitute_vars(config.goblint_conf_filepath, base_vars),
            '--enable', 'witness.yaml.enabled',
            '--set', 'witness.yaml.path', str(witness_filepath),
            injected_filepath
        ],
        shell=False,
        stdin=subprocess.DEVNULL,
        cwd=str(config.workdir_path)
    )

    subprocess.check_call(
        executable=config.witness_inject_cmd,
        args=[
            config.witness_inject_cmd,
            *substitute_vars_multi(config.cc_cflags, base_vars),
            '--witness-yaml', str(witness_filepath),
            '--assert-fn', config.assert_fn,
            str(injected_filepath)
        ],
        stdin=subprocess.DEVNULL,
        shell=False
    )

    subprocess.check_call(
        executable=config.clang_format_cmd,
        args=[
            config.clang_format_cmd,
            '-i', str(injected_filepath)
        ],
        stdin=subprocess.DEVNULL,
        shell=False
    )

    sarif_proc = subprocess.Popen(
        executable=config.cc_cmd,
        args=[
            config.cc_cmd,
            *substitute_vars_multi(config.cc_cflags, base_vars),
            *substitute_vars_multi(config.assert_cflags, base_vars),
            *substitute_vars_multi(config.sarif_cflags, base_vars),
            str(injected_filepath)
        ],
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

    segment_removal = ErroneousSegmentRemoval(assert_fn=config.assert_fn)
    segment_removal.load_sarif(sarif)
    with open(injected_clean_filepath, 'w') as injected_clean_file:
        segment_removal.process_file(injected_filepath, injected_clean_file)

    subprocess.check_call(
        executable=config.cc_cmd,
        args=[
            config.cc_cmd,
            *substitute_vars_multi(config.cc_cflags, base_vars),
            *substitute_vars_multi(config.assert_cflags, base_vars),
            '-o', str(exe_filepath),
            str(injected_clean_filepath)
        ]
    )

    run_cmd = substitute_vars(config.run_command, {
        **base_vars,
        'EXE': str(exe_filepath)
    })
    subprocess.check_call(
        run_cmd,
        shell=True
    )

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog=sys.argv[0], description='Witness Injection test runner')
    parser.add_argument('--generic-conf', type=str, required=True, help='Generic configuration file path')
    parser.add_argument('--goblint', type=str, required=True, help='Goblint')
    parser.add_argument('--cc', type=str, required=True, help='C compiler')
    parser.add_argument('--witness-inject', type=str, required=True, help='Witness injector')
    parser.add_argument('--clang-format', type=str, required=True, help='Clang format tool')
    parser.add_argument('--workdir', type=str, required=True, help='Working directory')
    parser.add_argument('test_filepath', help='Test .c file path')

    args = parser.parse_args(sys.argv[1:])
    test_filepath = pathlib.Path(args.test_filepath).resolve()
    test_conf_filepath = test_filepath.with_suffix('.json')
    if not test_conf_filepath.exists():
        test_conf_filepath = None
    generic_conf_filepath = pathlib.Path(args.generic_conf)
    workdir_path = pathlib.Path(args.workdir)
    workdir_path.mkdir(exist_ok=True)
    workdir_path = workdir_path.resolve()

    run_test(TestTaskConfig(
        test_filepath=test_filepath,
        cc_cmd=args.cc,
        goblint_cmd=args.goblint,
        witness_inject_cmd=args.witness_inject,
        clang_format_cmd=args.clang_format,
        workdir_path=workdir_path,
        test_conf_filepath=test_conf_filepath,
        generic_conf_filepath=generic_conf_filepath
    ).get_merged_conf())
