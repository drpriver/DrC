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
    subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

pip_wtf('gcovr')


def fixup_gcno_files(obj_dir: str) -> None:
    if not os.path.isdir(obj_dir):
        return
    master_gcnos: dict[str, str] = {}
    for entry in os.scandir(obj_dir):
        if entry.is_file() and entry.name.endswith('.gcno'):
            master_gcnos[entry.name] = os.path.abspath(entry.path)
    if not master_gcnos:
        return
    for root, _, files in os.walk(obj_dir):
        if os.path.abspath(root) == os.path.abspath(obj_dir):
            continue
        for file in files:
            if not file.endswith('.gcda'):
                continue
            gcno_name = file.replace('.gcda', '.gcno')
            target_gcno_path = os.path.join(root, gcno_name)
            if os.path.isfile(target_gcno_path):
                continue
            source_gcno = master_gcnos[gcno_name]
            if os.name == 'nt':
                os.link(source_gcno, target_gcno_path)
            else:
                rel_source = os.path.relpath(source_gcno, start=root)
                os.symlink(rel_source, target_gcno_path)

if __name__ == '__main__':
    obj_dir = "."
    for i, arg in enumerate(sys.argv):
        if arg.startswith('--object-directory='):
            obj_dir = arg.split('=', 1)[1]
            break
        elif arg == '--object-directory' and i + 1 < len(sys.argv):
            obj_dir = sys.argv[i + 1]
            break
    fixup_gcno_files(obj_dir)
    from gcovr.__main__ import main
    main()
