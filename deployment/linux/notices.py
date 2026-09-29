"""Collect notices for the shipped Qt dependency graph and baseline libraries."""
import json
from pathlib import Path
import re
import shutil


def stage(bundle, source, build, qt, sources, openssl, openssl_license=None):
    dest = bundle/'share/licenses'
    required = []
    def copy(src, name):
        target = dest/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, target)
        required.append(name)
    copy(source/'LICENSE', 'BrickSuite/LICENSE')
    for name in ['LICENSE.txt','COPYING','COPYING.LESSER','include/mcut/internal/cdt/LICENSE.txt']:
        copy(build/'_deps/mcut-src'/name, 'MCUT/'+name)
    copy(source/'cmake/BrickSuiteMcutLinuxQueue.cmake','MCUT/BrickSuiteMcutLinuxQueue.cmake')
    for name in ['LICENSE','Libraries/libressl/COPYING','submodules/zlib/LICENSE','submodules/libzip/LICENSE','submodules/cpp-base64/LICENSE','submodules/fast_float/LICENSE-MIT']:
        copy(build/'_deps/lib3mf-src'/name,'lib3mf/'+name)
    copy(openssl_license or sources/'openssl-3.5.8/LICENSE.txt','OpenSSL/LICENSE.txt')
    copy(sources/'icu-73.2-LICENSE','ICU/LICENSE')
    for package in ['zlib1g','libzstd1','libbrotli1']:
        copy(Path('/usr/share/doc')/package/'copyright',package+'/copyright')
    qtsource=sources/'qt-everywhere-src-6.10.3'
    selected = {p.name for p in (bundle/'lib').iterdir()}
    selected.update(p.name for p in (bundle/'plugins').rglob('*.so'))
    notices=[]
    for sbom in sorted((qt/'sbom').glob('*.spdx.json')):
        data=json.loads(sbom.read_text())
        files={f['SPDXID'] for f in data['files'] if Path(f['fileName']).name in selected}
        relations=data['relationships']
        ids={r['spdxElementId'] for r in relations if r['relationshipType']=='CONTAINS' and r['relatedSpdxElement'] in files}
        while True:
            more={r['relatedSpdxElement'] for r in relations if r['spdxElementId'] in ids and r['relationshipType'] in ['DEPENDS_ON','CONTAINS']}
            if more <= ids: break
            ids |= more
        packages=[p for p in data['packages'] if p['SPDXID'] in ids]
        notices.extend(packages)
        for package in packages:
            comment=package.get('comment','').replace('\\n','\n')
            match=re.search(r'/src_dir/(.*?/qt_attribution.json)',comment)
            index=re.search(r'Entry index: (\d+)',comment)
            if match and index:
                attribution=qtsource/match[1]
                entries=json.loads(attribution.read_text(),strict=False)
                if isinstance(entries,dict): entries=[entries]
                entry=entries[int(index[1])]
                names=entry.get('LicenseFile',[])
                if isinstance(names,str): names=[names]
                for name in names:
                    path=(attribution.parent/name).resolve()
                    copy(path,'Qt/'+str(path.relative_to(qtsource)))
    if not notices: raise ValueError('Qt shipped-component notice graph is empty')
    for name in ['LGPL-3.0-only.txt','GPL-3.0-only.txt','GPL-2.0-only.txt']:
        copy(qtsource/'LICENSES'/name,'Qt/'+name)
    (dest/'Qt/shipped-components.json').write_text(json.dumps(notices,indent=2)+'\n')
    required.append('Qt/shipped-components.json')
    return sorted(set(required))
