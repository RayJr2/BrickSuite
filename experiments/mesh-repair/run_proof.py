"""Isolated process runner. All mesh outputs stay in the explicitly chosen build folder."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xml.etree.ElementTree as ET
import zipfile
import numpy as np
import psutil
import trimesh


def executable(directory, name):
    return directory / (name + ('.exe' if os.name == 'nt' else ''))


def run(command, log, timeout=120, memory_mib=4096):
    started = time.perf_counter()
    peak = 0
    stopped = None
    with open(log, 'w') as output:
        process = subprocess.Popen(list(map(str, command)), stdout=output, stderr=subprocess.STDOUT)
        monitored = psutil.Process(process.pid)
        while process.poll() is None:
            try:
                info = monitored.memory_info()
                peak = max(peak, info.rss)
                # Commit limit as well as working set prevents paging from hiding a runaway.
                if max(info.rss, getattr(info, 'private', info.vms)) > memory_mib * 1024**2:
                    stopped = 'memory_limit'
            except psutil.NoSuchProcess:
                break
            if time.perf_counter() - started > timeout:
                stopped = 'timeout'
            if stopped:
                process.kill()  # Only this isolated proof child, never BrickSuite.
                break
            time.sleep(.01)
        code = process.wait()
    result = {'exit_code': code, 'wall_seconds': time.perf_counter()-started,
              'sampled_peak_rss_mib': peak/1024**2, 'stopped': stopped}
    for line in Path(log).read_text(errors='replace').splitlines():
        if line.startswith('{'):
            result['cgal'] = json.loads(line)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bin', required=True, type=Path)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--reference', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    results = {'limits': {'seconds': 120, 'memory_mib': 4096}, 'parts': {}}
    for part in ['23422', '44375a', '80910', '6553', '3001']:
        prefix = args.out / part
        subprocess.run([str(executable(args.bin,'source_dump')), str(args.library), part, str(prefix)], check=True, timeout=120)
        results['parts'][part] = {}
        for mode in ['inspect', 'clean', 'refine', 'fill', 'fill-clean', 'local']:
            target = args.out / f'{part}-{mode}.off'
            value = run([executable(args.bin,'cgal_repair'), str(prefix)+'-candidate.off', target, mode],
                        args.out/f'{part}-{mode}.log')
            results['parts'][part][mode] = value
            print(part, mode, json.dumps(value), flush=True)
            (args.out/'runs.json').write_text(json.dumps(results, indent=2))
    # Reference object geometry only: slicer component/build placement is recorded,
    # not mistaken for shape error. The original archive is never modified.
    with zipfile.ZipFile(args.reference) as archive:
        root = ET.fromstring(archive.read('3D/3dmodel.model'))
    meshes = root.findall('.//{*}mesh')
    if len(meshes) != 1 or root.attrib.get('unit', 'millimeter') != 'millimeter':
        raise ValueError('This proof expects one mesh object in millimetres; inspect other archives explicitly.')
    vertices = np.array([[float(v.attrib[k]) for k in ('x','y','z')] for v in meshes[0].findall('.//{*}vertex')])
    faces = np.array([[int(t.attrib[k]) for k in ('v1','v2','v3')] for t in meshes[0].findall('.//{*}triangle')])
    reference = trimesh.Trimesh(vertices=vertices, faces=faces, process=False)
    reference.export(args.out/'23422-bambu-raw.off')
    results['reference'] = {'sha256': hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        'raw_bounds_mm': reference.bounds.tolist(), 'raw_extents_mm': reference.extents.tolist(),
        'placement': [x.attrib for x in root.findall('.//{*}component')+root.findall('.//{*}item')]}
    results['reference']['inspection'] = run([executable(args.bin,'cgal_repair'),args.out/'23422-bambu-raw.off',
        args.out/'23422-bambu-inspected.off','inspect'], args.out/'23422-bambu-inspect.log')
    (args.out/'runs.json').write_text(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
