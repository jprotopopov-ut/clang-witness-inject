import os
import pathlib
import subprocess
import signal
import dataclasses
from typing import Optional, List, Dict, Iterable

@dataclasses.dataclass
class AFLppFuzzTask:
    executable: pathlib.Path
    executable_args: List[str]
    executable_env: Dict[str, str]
    input_dir: pathlib.Path

@dataclasses.dataclass
class AFLppFuzzConfig:
    aflpp_dir: pathlib.Path
    skip_cpufreq: bool
    quiet: bool

class AFLppFuzzCtrl:
    def __init__(self, *, out_dir: pathlib.Path, config: AFLppFuzzConfig):
        self._config = config
        self._out_dir = out_dir
        self._task = None
        self._aflpp_fuzz_proc: Optional[subprocess.Popen] = None

    @property
    def config(self) -> AFLppFuzzConfig:
        return self._config
    
    @property
    def out_dir(self) -> pathlib.Path:
        return self._out_dir
    
    @property
    def last_task(self) -> Optional[AFLppFuzzTask]:
        return self._task
    
    @property
    def is_running(self) -> bool:
        return self._aflpp_fuzz_proc is not None and self._aflpp_fuzz_proc.poll() is None
    
    @property
    def failed(self) -> bool:
        return self._aflpp_fuzz_proc is not None and self._aflpp_fuzz_proc.poll()
    
    @property
    def crashes(self) -> Iterable[pathlib.Path]:
        yield from self._out_dir.glob('default/crashes*/id*')

    @property
    def has_crashes(self) -> bool:
        return next(self.crashes, None) is not None

    @property
    def crash_count(self) -> bool:
        return sum(1 for _ in self.crashes)
    
    def stop(self, *, timeout: Optional[float] = None):
        if self._aflpp_fuzz_proc is None:
            raise RuntimeError('AFL++ fuzz process is not running')
        
        if  self._aflpp_fuzz_proc.poll() is None:
            self._aflpp_fuzz_proc.send_signal(signal.SIGINT)
            try:
                self._aflpp_fuzz_proc.wait(timeout)
            except subprocess.TimeoutExpired:
                self._aflpp_fuzz_proc.kill()
        self._aflpp_fuzz_proc = None

    def _run_aflpp_fuzz(self, *, task: AFLppFuzzTask, resume: bool):
        if self.is_running:
            raise RuntimeError('AFL++ fuzz process is already running')
        
        afl_fuzz_exe = str(self.config.aflpp_dir.resolve() / 'bin' / 'afl-fuzz')

        env = dict(os.environ)
        env['AFL_SKIP_CPUFREQ'] = '1' if self.config.skip_cpufreq else '0'
        env.update(task.executable_env)

        self._aflpp_fuzz_proc = subprocess.Popen(
            executable=afl_fuzz_exe,
            args=[
                afl_fuzz_exe,
                '-i', str(task.input_dir.resolve()) if not resume else '-',
                '-o', str(self.out_dir.resolve()),
                '--',
                str(task.executable.resolve()),
                *task.executable_args
            ],
            shell=False,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL if self.config.quiet else None,
            stderr=subprocess.DEVNULL if self.config.quiet else None,
            env=env
        )
        self._task = task

    def start(self, *, task: AFLppFuzzTask):
        return self._run_aflpp_fuzz(task=task, resume=False)

    def resume(self, *, task: Optional[AFLppFuzzTask] = None):
        if task is None:
            task = self.last_task
        if task is None:
            raise RuntimeError('Unable to resume AFL++ fuzz')
        
        return self._run_aflpp_fuzz(task=task, resume=True)

    def __enter__(self, *args, **kwargs):
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        if self._aflpp_fuzz_proc is not None:
            self.stop()

        if exc_value is not None:
            raise exc_value
        else:
            return self