"""Use the CGAL proof's unchanged measurement contracts for fair comparisons."""
import argparse
import json
from pathlib import Path
import sys
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'mesh-repair'))
from analyze import load, topology, directed, ownership, features, plot
from run_proof import run, executable


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--prior', type=Path, required=True)
    parser.add_argument('--cgal-bin', type=Path, required=True)
    args = parser.parse_args()
    root = args.out
    report = {'method':'unchanged fixed-seed area samples plus barycentric grid; mm',
              'topology':{}, 'comparisons':{}, 'ownership':{}, 'features':{}}
    meshes = {p.stem:load(p) for p in root.glob('*.off') if 'inspected' not in p.stem}
    for name, mesh in meshes.items():
        if len(mesh.faces):
            report['topology'][name] = topology(mesh)
            if name.startswith('23422'):
                report['features'][name] = features(mesh)
    for part in ['23422','6553','44375a','80910','3001']:
        meshes[part+'-source'] = load(args.prior/f'{part}-source.off')
    pairs = [('23422-studio','23422-bambu'),('23422-studio','23422-source'),('23422-bambu','23422-source')]
    for mode in ['outer','fill-outer','outer-fill','cgal-fill','cgal-local']:
        pairs += [('23422-studio-'+mode,'23422-studio'),('23422-studio-'+mode,'23422-source'),
                  ('23422-studio-'+mode,'23422-bambu')]
    pairs += [(p+'-fill-outer',p+'-source') for p in ['23422','6553','44375a','80910','3001']]
    pairs += [('3001-outer','3001-source')]
    for a,b in pairs:
        if a not in meshes or not len(meshes[a].faces):
            continue
        count = 20000 if a.startswith('23422') else 5000
        key = a+'__'+b
        report['comparisons'][key] = {'forward':directed(meshes[a],meshes[b],count),
                                       'reverse':directed(meshes[b],meshes[a],count)}
        print(key, 'distances complete', flush=True)
        (root/'analysis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    for name in ['23422-studio','23422-bambu','23422-studio-outer','23422-studio-fill-outer',
                 '23422-fill-outer','6553-fill-outer','44375a-fill-outer','80910-fill-outer','3001-outer']:
        part = name.split('-')[0]
        ancestry = json.loads((args.prior/f'{part}-ancestry.json').read_text(encoding='utf-8'))
        report['ownership'][name] = ownership(meshes[name],meshes[part+'-source'],ancestry)
        print(name, 'ownership complete', flush=True)
    (root/'analysis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    plot({'Installed LDraw':meshes['23422-source'], 'Studio original (mm)':meshes['23422-studio'],
          'Bambu reference (mm)':meshes['23422-bambu'], 'Geogram fill then outer':meshes['23422-studio-fill-outer'],
          'Geogram outer only':meshes['23422-studio-outer'], 'CGAL local same OBJ':meshes['23422-studio-cgal-local']},
         root/'23422-comparison.png')
    import matplotlib.pyplot as plt
    import trimesh
    fig, axes = plt.subplots(2,3,figsize=(15,8))
    for col,(name,title) in enumerate([('23422-studio','Studio original'),('23422-bambu','Bambu'),
                                      ('23422-studio-fill-outer','Geogram fill then outer')]):
        for row,z in enumerate([.123,2.5]):
            axis = axes[row,col]
            for model,color,label in [(meshes['23422-source'],'#777777','Installed LDraw'),(meshes[name],'#d34d28',title)]:
                for j,line in enumerate(trimesh.intersections.mesh_plane(model,[0,0,1],[0,0,z])):
                    axis.plot(line[:,0],line[:,1],color=color,linewidth=1,label=label if j==0 else None)
            axis.set_aspect('equal');axis.set_title(f'{title}, Z={z} mm');axis.legend()
    fig.tight_layout();fig.savefig(root/'23422-sections.png',dpi=160);plt.close(fig)
    bounded = {}
    # Independent global estimate: fixed 0.001 mm error, identical to CGAL proof.
    for a,b in pairs:
        a_path = root/f'{a}.off'
        b_path = args.prior/f'{b}.off' if b.endswith('-source') else root/f'{b}.off'
        if not a_path.exists() or not len(meshes[a].faces):
            continue
        key = a+'__'+b
        bounded[key] = run([executable(args.cgal_bin,'cgal_distance'),a_path,b_path],root/f'distance-{key}.log')
        (root/'bounded-distances.json').write_text(json.dumps(bounded,indent=2),encoding='utf-8')
        print(key, 'bounded estimate complete', flush=True)


if __name__ == '__main__':
    main()
