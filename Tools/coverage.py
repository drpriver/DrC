#!/usr/bin/env python3
from __future__ import annotations

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
    subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

pip_wtf('gcovr')
if __name__ == '__main__':
    from gcovr.__main__ import main
    main()
