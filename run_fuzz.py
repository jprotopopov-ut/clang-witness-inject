#!/usr/bin/env python3
import sys
import os
import json
import argparse
import dataclasses
import pathlib
import enum
from typing import Optional, List, Dict, Iterable

def interpolate_env_into(env: Dict[str, str], filepath: str) -> str:
    for key, value in env.items():
        filepath = filepath.replace(f'${key}', value)
    return str

def get_optional_path(filepath: Optional[str], default: Optional[pathlib.Path] = None) -> Optional[pathlib.Path]:
    if filepath is not None:
        return pathlib.Path(filepath).resolve()
    else:
        return default
    
def get_optional_interpolated_path(env: Dict[str, str], filepath: Optional[str], default: Optional[pathlib.Path] = None) -> Optional[pathlib.Path]:
    if filepath is not None:
        return pathlib.Path(interpolate_env_into(env, filepath)).resolve()
    else:
        return default
    
def interpolate_iter(env: Dict[str, str], items: Iterable[str]) -> Iterable[str]:
    for entry in items:
        yield interpolate_env_into(env, entry)

@dataclasses.dataclass
class FuzzToolsConfig:
    aflpp_dir: pathlib.Path
    witness_inject: Optional[pathlib.Path]
    clang_cc: Optional[pathlib.Path]
    clang_format: Optional[pathlib.Path]

    @staticmethod
    def load(config, env: Dict[str, str]) -> 'FuzzToolsConfig':            
        return FuzzToolsConfig(
            aflpp_dir=pathlib.Path(interpolate_env_into(env, config['aflpp_dir'])).resolve(),
            witness_inject=get_optional_interpolated_path(env, config.get('witness_inject')),
            clang_cc=get_optional_interpolated_path(env, config.get('clang_cc')),
            clang_format=get_optional_interpolated_path(env, config, 'clang_format'),
        )
    
class FuzzProgramMode:
    SourceCode = 'source'
    Executable = 'executable'

class FuzzProgramSanitizer(enum.Enum):
    Disabled = 'none'
    Address = 'asan'
    UndefinedBehavior = 'ubsan'

@dataclasses.dataclass
class FuzzProgramCompilationConfig:
    cflags: List[str]
    ldflags: List[str]
    sanitizer: FuzzProgramSanitizer
    witness_yaml: Optional[pathlib.Path]

    @staticmethod
    def load(env: Dict[str, str], config) -> 'FuzzProgramCompilationConfig':
        return FuzzProgramCompilationConfig(
            cflags=list(interpolate_iter(env, config.get('cflags', ()))),
            ldflags=list(interpolate_iter(env, config.get('ldflags', ()))),
            sanitizer=FuzzProgramSanitizer(config.get('sanitizer', FuzzProgramSanitizer.Disabled.value)),
            witness_yaml=get_optional_interpolated_path(env, config.get('witness_yaml'))
        )

@dataclasses.dataclass
class FuzzProgramConfig:
    filepath: pathlib.Path
    mode: FuzzProgramMode
    compilation_config: Optional[FuzzProgramCompilationConfig]

    @staticmethod
    def load(config) -> 'FuzzProgramConfig':
        mode = FuzzProgramMode(config['mode'])
        return FuzzProgramConfig(
            filepath=pathlib.Path(interpolate_env_into(env, config['program'])),
            mode=mode,
            compilation_config=FuzzProgramCompilationConfig.load(config) if mode == FuzzProgramMode.SourceCode else None
        )

@dataclasses.dataclass
class FuzzConfig:
    tools: FuzzToolsConfig
    program: FuzzProgramConfig

    @staticmethod
    def load(config, env: Dict[str, str]) -> 'FuzzConfig':
        return FuzzConfig(
            tools=FuzzToolsConfig.load(config['tools'], env),
            program=FuzzProgramConfig.load(config['program'])
        )

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog=sys.argv[0])
    parser.add_argument('config', type=str, help='Fuzz configuration')

    args = parser.parse_args(sys.argv[1:])
    with open(args.config) as config_file:
        config = FuzzConfig.load(json.load(config_file), dict(os.environ))
