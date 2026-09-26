"""Detailed checks of sensitivity cases and localization of unmatched regions."""
import argparse
import json
from pathlib import Path
import sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'mesh-repair'))
from analyze import load, directed, ownership, topology, features
from run_proof import executable, run

parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--prior', type=Path, required=True)
parser.add_argument('--cgal-bin', type=Path, required=True)
args = parser.parse_args()
report = {}
for name in ['23422-studio-simplify','23422-simplify','6553-repeat2','44375a-simplify','80910-simplify','3001-simplify']:
    part = name.split('-')[0]
    path = args.out/f'{name}.off'
    source_path = args.prior/f'{part}-source.off'
    mesh, source = load(path), load(source_path)
    ancestry = json.loads((args.prior/f'{part}-ancestry.json').read_text(encoding='utf-8'))
    item = {'topology':topology(mesh),'to_source':directed(mesh,source,20000 if part=='23422' else 5000),
            'source_to':directed(source,mesh,20000 if part=='23422' else 5000),
            'ownership':ownership(mesh,source,ancestry)}
    item['bounded'] = run([executable(args.cgal_bin,'cgal_distance'),path,source_path],args.out/f'distance-{name}-source.log')
    if part == '23422':
        item['features'] = features(mesh)
    if name.startswith('23422-studio'):
        bambu = load(args.out/'23422-bambu.off')
        item['to_bambu'] = directed(mesh,bambu)
        item['bambu_to'] = directed(bambu,mesh)
    report[name] = item
    print(name, 'complete', flush=True)
    (args.out/'supplement.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

# Distinguish Studio ancestry support from authoritative LDraw ancestry.
studio = load(args.out/'23422-studio.off')
synthetic_ids = [{'file':'Studio OBJ position-face', 'line':i, 'references':[]} for i in range(len(studio.faces))]
report['studio_input_support'] = {name:ownership(load(args.out/f'{name}.off'),studio,synthetic_ids)
    for name in ['23422-studio-fill-outer','23422-studio-simplify']}
report['boundary_locations'] = {}
for name in ['23422-studio-fill-outer','23422-studio-simplify']:
    mesh = load(args.out/f'{name}.off')
    counts = np.bincount(mesh.edges_unique_inverse)
    edge_points = mesh.vertices[mesh.edges_unique[counts==1]]
    centers = edge_points.mean(axis=1)
    report['boundary_locations'][name] = {'bounds_mm':[edge_points.min(axis=(0,1)).tolist(),edge_points.max(axis=(0,1)).tolist()],
        'left_arm_edges':int((centers[:,0]<-4).sum()),'body_edges':int((abs(centers[:,0])<=4).sum()),
        'right_arm_edges':int((centers[:,0]>4).sum()),
        'total_length_mm':float(np.linalg.norm(edge_points[:,0]-edge_points[:,1],axis=1).sum())}
(args.out/'supplement.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
