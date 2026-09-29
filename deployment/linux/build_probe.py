#!/usr/bin/env python3
"""Relink disposable probes from baseline production objects; no product CLI added."""
import argparse
from pathlib import Path
import shlex
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compile-only',action='store_true');p.add_argument('--build',type=Path,default=Path('/work/build'));p.add_argument('--output',type=Path,default=Path('/work/probes'));args=p.parse_args()
    repo=Path(__file__).resolve().parents[2];qt=Path('/opt/qt');args.output.mkdir(parents=True,exist_ok=True)
    lines=subprocess.check_output(['ninja','-t','commands','BrickSuite'],cwd=args.build,text=True).splitlines()
    original=shlex.split(lines[-1].split('&&')[1].strip())
    for name in ['PackageProbe','NativeProbe']:
        obj=args.output/(name+'.o')
        flags=['g++-11','-std=c++17','-fPIC','-DNDEBUG','-DQT_WIDGETS_LIB','-DQT_GUI_LIB','-O2','-ffile-prefix-map=/src=.','-ffile-prefix-map=/work=.','-ffile-prefix-map=/opt/qt=.','-I'+str(repo),'-I'+str(qt/'include')]
        flags+=['-I'+str(qt/'include'/m) for m in ['QtCore','QtGui','QtWidgets','QtNetwork','QtSql','QtOpenGL','QtOpenGLWidgets']]
        subprocess.run([*flags,'-c',str(Path(__file__).with_name(name+'.cpp')),'-o',str(obj)],check=True)
        if args.compile_only:continue
        link=[str(obj) if x=='CMakeFiles/BrickSuite.dir/src/main.cpp.o' else x for x in original]
        link[link.index('-o')+1]=str(args.output/name)
        subprocess.run(link,cwd=args.build,check=True)
if __name__=='__main__':main()
