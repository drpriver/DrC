#!/usr/bin/env python3
from __future__ import annotations
import os
import sys

# Modified from https://pip.wtf
def pip_wtf(command:list[str]|str) -> None:
    import os, os.path, sys, subprocess, shlex
    if isinstance(command, str):
        command = shlex.split(command)
    here = os.path.dirname(os.path.abspath(__file__))
    base = os.path.basename(__file__)
    cache = os.path.normpath(os.path.join(here, 'pymodules', base+'.modules'))
    sys.path = [p for p in sys.path if "-packages" not in p] + [cache]
    os.environ["PATH"] = cache + os.path.sep + "bin" + os.pathsep + os.environ["PATH"]
    os.environ["PYTHONPATH"] = os.pathsep.join(sys.path)
    if os.path.exists(cache): return
    cmd = [sys.executable, '-m', 'pip', 'install', '-t', cache, *command]
    subprocess.check_call(cmd)

pip_wtf('pydndc>=1.6.1')


if __name__ == '__main__':
    from pydndc.__main__ import main
    sys.exit(main())
