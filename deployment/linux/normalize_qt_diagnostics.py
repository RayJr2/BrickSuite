"""Remove two known vendor __FILE__ prefixes without moving ELF data or code.

Qt 6.10.3 qtbase uses these strings in warning calls in qdatetimeedit.cpp and
qabstractspinbox.cpp. They are not runtime lookup paths. Equal-length replacement
preserves all offsets, addresses, suffix sharing and NUL terminators. Unexpected
inputs fail before the file is written. This recipe accompanies the LGPL runtime.
"""
from pathlib import Path
import subprocess

PREFIX=b'/home/qt/work/qt/'
RELATIVE=b'./qt6-source-dir/'
FILES=[b'qtbase/src/widgets/widgets/qdatetimeedit.cpp',
       b'qtbase/src/widgets/widgets/qabstractspinbox.cpp']


def normalize(path):
    path=Path(path)
    if path.name!='libQt6Widgets.so.6.10.3':return []
    data=path.read_bytes();size=len(data);records=[]
    sections=subprocess.check_output(['readelf','-SW',str(path)],text=True)
    fields=next((line.split() for line in sections.splitlines() if ' .rodata ' in line),None)
    if not fields:raise ValueError('Qt read-only diagnostic section missing')
    at=fields.index('.rodata');start=int(fields[at+3],16);end=start+int(fields[at+4],16)
    if fields[at+1]!='PROGBITS' or any(flag in fields[at+6] for flag in 'WX'):raise ValueError('Qt diagnostic section is not read-only data')
    if len(PREFIX)!=len(RELATIVE):raise ValueError('Diagnostic prefix length mismatch')
    for source in FILES:
        old=b'\0'+PREFIX+source+b'\0';new=b'\0'+RELATIVE+source+b'\0'
        if data.count(old)!=1:raise ValueError('Unexpected Qt diagnostic string inventory')
        offset=data.index(old)+1
        if not start<=offset or offset+len(old)-2>end:raise ValueError('Qt diagnostic outside read-only data')
        data=data.replace(old,new)
        records.append({'offset':offset,'section':'.rodata','old':(PREFIX+source).decode(),'new':(RELATIVE+source).decode()})
    if len(data)!=size or PREFIX in data:raise ValueError('Unexpected Qt diagnostic remapping result')
    path.write_bytes(data)
    return records
