"""Run the public C++ behavior tests using a C++17 compiler."""
import os,shutil,subprocess,tempfile
from pathlib import Path

root=Path(__file__).resolve().parent.parent
compiler=os.environ.get('CXX') or shutil.which('cl') or shutil.which('g++') or shutil.which('clang++')
if not compiler:raise SystemExit('Put a C++17 compiler on PATH; MSVC requires a Developer Command Prompt.')
include=root/'firmware/main/boards/esp-sparkbot'
tests=sorted((root/'firmware/tests').glob('*_test.cpp'))
if not tests:raise SystemExit('No public tests found')
msvc=Path(compiler).name.lower() in ['cl','cl.exe']
with tempfile.TemporaryDirectory(prefix='sparkbot-tests-') as folder:
    out=Path(folder)
    for source in tests:
        executable=out/(source.stem+('.exe' if os.name=='nt' else ''))
        cmd=[compiler,'/nologo','/std:c++17','/EHsc','/utf-8','/I'+str(include),str(source),'/Fe:'+str(executable),'/Fo:'+str(out/(source.stem+'.obj'))] if msvc else [compiler,'-std=c++17','-I',str(include),str(source),'-o',str(executable)]
        subprocess.run(cmd,check=True,cwd=out)
        subprocess.run([str(executable)],check=True,cwd=root)
print('PASS:',len(tests),'public behavior test programs')
