"""Combine independent checks and produce the residual-boundary diagnostic figure."""
import argparse
import json
from pathlib import Path
import sys
import numpy as np
import trimesh
from worker import durable_json
from core import SOURCE_EPS_MM
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'mesh-repair'))
from analyze import load


def json_log(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8',errors='replace').splitlines()
            if line.startswith('{')][-1]


parser = argparse.ArgumentParser()
parser.add_argument('--out',type=Path,required=True)
args = parser.parse_args()
summary = json.loads((args.out/'run-summary.json').read_text(encoding='utf-8'))
aggregate = {}
for part,entry in summary['parts'].items():
    directory = args.out/part
    report = json.loads(Path(entry['result']).read_text(encoding='utf-8'))
    check = json_log(directory/'exact-constrained'/'worker.log')
    report['exact_check'] = check
    report['original_exact_check'] = json_log(directory/'exact-original'/'worker.log')
    report['global_distances'] = {p.name.removeprefix('distance-'):json_log(p/'worker.log')
                                  for p in directory.glob('distance-*')}
    distance = report['comparisons']['authoritative']
    sampled_exceeds = max(distance[d]['sampled_max_mm'] for d in ['repair_to','to_repair']) > SOURCE_EPS_MM
    report['acceptance'].update(zero_exact_intersection_or_contact_pairs=check['self_intersection_pairs']==0,
        experimental_source_screen_epsilon_mm=SOURCE_EPS_MM,
        raw_source_distance_screen='exceeds existing seam tolerance; deleted internal faces not excused without evidence' if sampled_exceeds else 'sampling alone is not a certificate',
        explicit_face_provenance=True,
        reason='Full feature/opening and exterior material classification is not certified; topology and distance failures are retained independently.')
    # Deliberately fail closed: none of these runs has a feature/exterior certificate.
    gates = ['one_component','zero_boundary_edges','zero_nonmanifold_edges','zero_near_degenerate_faces',
             'zero_exact_intersection_or_contact_pairs','strict_bidirectional_source_fidelity_proven',
             'intentional_openings_and_global_wall_thickness_proven','explicit_face_provenance']
    report['acceptance']['accepted'] = all(report['acceptance'][gate] for gate in gates)
    report['acceptance']['failed_or_unproven_gates'] = [gate for gate in gates if not report['acceptance'][gate]]
    durable_json(Path(entry['result']),report)
    aggregate[part] = report
durable_json(args.out/'validated-summary.json',aggregate)

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
config = summary['parts']['23422']['config']
mesh,source = load(config['residual']),load(config['authoritative'])
diagnostic = json.loads((args.out/'23422/residual-analysis/boundaries.json').read_text(encoding='utf-8'))
fig,axes = plt.subplots(1,2,figsize=(12,6))
for axis,z,title in zip(axes,[3.999,-3.999],['Top rim, Z approximately +4 mm','Bottom rim, Z approximately -4 mm']):
    for model,color,label in [(source,'#888888','Installed LDraw section'),(mesh,'#1f77b4','Studio / saved residual section')]:
        lines = trimesh.intersections.mesh_plane(model,[0,0,1],[0,0,z])
        for j,line in enumerate(lines):
            axis.plot(line[:,0],line[:,1],color=color,linewidth=1,label=label if j==0 else None)
    for path in diagnostic['loops']:
        points = mesh.vertices[path['vertices']]
        if np.sign(points[:,2].mean())!=np.sign(z):
            continue
        for e in path['edges']:
            p = mesh.vertices[diagnostic['boundary_edges'][e]['vertices']]
            axis.plot(p[:,0],p[:,1],color='#d62728',linewidth=2)
    axis.plot([],[],color='#d62728',linewidth=2,label='Ambiguous residual boundaries')
    axis.set(xlim=(-4.3,4.3),ylim=(-4.3,4.3),xlabel='X mm',ylabel='Y mm',title=title)
    axis.set_aspect('equal');axis.legend(loc='lower center',fontsize=8)
fig.suptitle('23422: boundaries are at the outer rims; no center-bore cap is authorized')
fig.tight_layout();fig.savefig(args.out/'23422-boundary-rims.png',dpi=160);plt.close(fig)
