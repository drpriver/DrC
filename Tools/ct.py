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

pip_wtf('clangtags')
if __name__  == '__main__':
    import sys
    # sys.argv.extend(['--pool_size', '0'])
    import clangtags.__main__
    # clangtags.__main__.main()
    with open('.vimrc') as fp:
        v = fp.read()
    line1, rest = v.split('\n', 1)
    assert line1.startswith('set path=')
    paths = line1[len('set path='):].split(',')
    filtered = []
    for p in paths:
        if 'Vendored' in p:
            continue
        if 'Resources' in p:
            continue
        if 'build' in p:
            continue
        if 'Fetched' in p:
            continue
        if 'pymodules' in p:
            continue
        if '.app' in p:
            continue
        if 'Dist' in p:
            continue
        if 'node' in p:
            continue
        filtered.append(p)
    v = 'set path='+ ','.join(filtered) + '\n' + rest
    with open('.vimrc', 'w') as fp:
        fp.write(v)
        fp.write('\nau BufEnter *.txt set ts=2 sts=2 sw=2 et foldmethod=indent\n')
        fp.write('au BufEnter *.c,*.h,*.m,*.mm syn keyword cType SER SER_EXCLUDE')
