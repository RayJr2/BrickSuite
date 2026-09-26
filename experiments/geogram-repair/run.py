"""Fixed corpus, bounded child processes, and explicitly normalized matched pair."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET
import zipfile
import numpy as np
import trimesh

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'mesh-repair'))
from analyze import align, topology
from run_proof import run, executable


def read_obj(path):
    # Preserve OBJ position indexing. UV/normal seams must not duplicate vertices.
    vertices, faces = [], []
    for line in path.read_text(encoding='utf-8').splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == 'v':
            vertices.append([float(x) for x in fields[1:4]])
        elif fields[0] == 'f':
            if len(fields) != 4:
                raise ValueError('Benchmark must already be triangulated')
            indices = [int(x.split('/')[0]) for x in fields[1:]]
            faces.append([x-1 if x > 0 else len(vertices)+x for x in indices])
    return trimesh.Trimesh(vertices=vertices, faces=faces, process=False)


def read_reference(path):
    with zipfile.ZipFile(path) as archive:
        root = ET.fromstring(archive.read('3D/3dmodel.model'))
    meshes = root.findall('.//{*}mesh')
    if len(meshes) != 1 or root.attrib.get('unit', 'millimeter') != 'millimeter':
        raise ValueError('Expected one benchmark mesh with millimeter metadata')
    placement = [x.attrib for x in root.findall('.//{*}component')+root.findall('.//{*}item')]
    for item in placement:
        if 'transform' in item:
            transform = np.array([float(x) for x in item['transform'].split()])
            if not np.allclose(transform[:9].reshape(3,3), np.eye(3), atol=1e-12):
                raise ValueError('Nontranslation placement requires explicit inspection')
    vertices = [[float(v.attrib[k]) for k in ('x','y','z')] for v in meshes[0].findall('.//{*}vertex')]
    faces = [[int(f.attrib[k]) for k in ('v1','v2','v3')] for f in meshes[0].findall('.//{*}triangle')]
    return trimesh.Trimesh(vertices=vertices, faces=faces, process=False), placement


def save(mesh, path):
    # trimesh's OFF writer defaults to 10 decimal places; retain input precision.
    with path.open('w', encoding='utf-8') as output:
        output.write(f'OFF\n{len(mesh.vertices)} {len(mesh.faces)} 0\n')
        for p in mesh.vertices:
            output.write(' '.join(format(x, '.17g') for x in p)+'\n')
        for f in mesh.faces:
            output.write('3 '+' '.join(map(str, f))+'\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bin', type=Path, required=True)
    parser.add_argument('--cgal-bin', type=Path, required=True)
    parser.add_argument('--prior', type=Path, required=True)
    parser.add_argument('--obj', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if not args.obj.is_file() or not args.reference.is_file():
        raise FileNotFoundError('Both exact user benchmark files are required')
    args.out.mkdir(parents=True, exist_ok=True)
    source = trimesh.load_mesh(args.prior/'23422-source.off', process=False)
    obj = read_obj(args.obj)
    reference, placement = read_reference(args.reference)
    report = {'normalization_mm_per_studio_unit': .4, 'placement': placement,
              'inputs': {}, 'runs': {}, 'limits': {'seconds':120,'memory_mib':4096}}
    for name, mesh, path in [('studio',obj,args.obj),('bambu',reference,args.reference)]:
        normalized, registration = align(mesh, source)
        save(normalized, args.out/f'23422-{name}.off')
        report['inputs'][name] = {'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'raw_vertices':len(mesh.vertices),'raw_faces':len(mesh.faces),
            'raw_dimensions_studio_units':mesh.extents.tolist(),
            'normalization':registration, 'normalized':topology(normalized)}
    inputs = {'23422-studio':args.out/'23422-studio.off'}
    inputs.update({p:args.prior/f'{p}-candidate.off' for p in ['23422','6553','44375a','80910','3001']})
    def record(key, command):
        value = run(command, args.out/f'{key}.log')
        if 'cgal' in value:
            value['engine'] = value.pop('cgal')
        report['runs'][key] = value
        (args.out/'runs.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(key, json.dumps(value), flush=True)
    for name, path in inputs.items():
        for mode in ['clean','arrange','outer','fill-outer','outer-fill']:
            key = f'{name}-{mode}'
            output = args.out/f'{key}.off'
            record(key, [executable(args.bin,'geogram_repair'),path,output,mode,'0.0004'])
            if report['runs'][key]['exit_code'] == 0 and output.is_file():
                record(key+'-check', [executable(args.cgal_bin,'cgal_repair'),output,
                    args.out/f'{key}-inspected.off','inspect'])
    # Same input and prior algorithms: avoids attributing input differences to engines.
    for mode in ['clean','refine','fill','fill-clean','local']:
        key = '23422-studio-cgal-'+mode
        record(key, [executable(args.cgal_bin,'cgal_repair'),args.out/'23422-studio.off',
                     args.out/f'{key}.off',mode])


if __name__ == '__main__':
    main()
