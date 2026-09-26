import csv,json,sys,hashlib
from pathlib import Path
import numpy as np
import trimesh
from certificate import certify,convex_source_cells
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/source-exposure-proof'
sys.path.insert(0,str(ROOT/'experiments/asymmetric-fidelity'))
from proof import protected_faces,nearest
source=trimesh.load_mesh(ROOT/'build/blender-repair-proof/authoritative-source.off',process=False)
old=trimesh.load_mesh(ROOT/'build/mesh-repair-proof/results/23422-source.off',process=False)
assert np.array_equal(source.triangles,old.triangles)
ancestry=json.loads((ROOT/'build/mesh-repair-proof/results/23422-ancestry.json').read_text())
protected,regions=protected_faces(source,ancestry)
# Exact coordinate indexing only; don't introduce a near-distance source closure.
vertices,inverse=np.unique(source.vertices,axis=0,return_inverse=True)
indexed=trimesh.Trimesh(vertices=vertices,faces=inverse[source.faces],process=False)
cells=convex_source_cells(indexed)
rows=list(csv.DictReader((ROOT/'build/asymmetric-fidelity-proof/blender-outliers.csv').open()))
assert len(rows)==1086
results=[]
for row in rows:
    index=int(row['triangle']);p=np.array([float(row[c]) for c in ('x','y','z')])
    item=certify(indexed,p,source.face_normals[index],bool(protected[index]),cells)
    if item.get('normal_blocker_triangle') is not None:
        blocker=item['normal_blocker_triangle'];item['normal_blocker_ancestry']=ancestry[blocker]
        item['normal_blocker_region']=regions[blocker]
        item['normal_blocker_material_exit']=bool(np.dot(source.face_normals[blocker],source.face_normals[index])>1e-8)
    results.append({**row,**item})
report={'source_triangles':len(source.faces),'closed_convex_source_cells':len(cells),'hemisphere_directions':65,
        'source_only':True,'classification':{},'groups':{},'would_accept':False}
for group in ('arm_body','spline_passage'):
    subset=[r for r in results if (r['region']=='authored_passage_wall')==(group=='spline_passage')]
    report['groups'][group]={state:sum(r['classification']==state for r in subset) for state in ('certified_internal','required_exterior','protected_exposed','ambiguous')}
for state in ('certified_internal','required_exterior','protected_exposed','ambiguous'):
    report['classification'][state]=sum(r['classification']==state for r in results)
report['would_accept']=all(r['classification']=='certified_internal' for r in results)
report['stable_escape_sample_count']=sum(r.get('stable_escape_directions',0)>0 for r in results)
report['normal_blocker_regions']={region:sum(r.get('normal_blocker_region')==region for r in results) for region in sorted(set(regions))}
report['normal_material_exit_count']=sum(r.get('normal_blocker_material_exit',False) for r in results)
report['protected_centroid_probes']={}
for region in ('authored_passage_wall','authored_internal_spline','opening_rim'):
    probes=[]
    for i,r in enumerate(regions):
        if r==region:
            c=certify(indexed,source.triangles_center[i],source.face_normals[i],True,cells)
            probes.append({'triangle':i,**c})
    report['protected_centroid_probes'][region]={state:sum(p['classification']==state for p in probes) for state in ('protected_exposed','ambiguous')}
    report['protected_centroid_probes'][region]['samples']=probes
report['source_sha256']=hashlib.sha256((ROOT/'build/blender-repair-proof/authoritative-source.off').read_bytes()).hexdigest()
# Only now evaluate preservation; the source certificate above never saw repair geometry.
repair=trimesh.load_mesh(ROOT/'build/blender-repair-proof/23422-blender-repaired.off',process=False)
repair.vertices+=source.bounds.mean(0)-repair.bounds.mean(0)
for region,group in report['protected_centroid_probes'].items():
    exposed=[p['triangle'] for p in group['samples'] if p['classification']=='protected_exposed']
    if exposed:
        _,distances,_=nearest(repair,source.triangles_center[exposed])
        group['maximum_exposed_probe_deviation_mm']=float(distances.max())
        group['exposed_probe_loss_count']=int(np.sum(distances>.20))
(OUT/'certificate.json').write_text(json.dumps({'summary':report,'samples':results},indent=2))
print(json.dumps(report,indent=2))
