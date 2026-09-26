"""Child stage: classify residuals, attempt only proven seams, write full provenance."""
import argparse
import json
from pathlib import Path
import sys
import numpy as np
import trimesh
from core import supports, classify_and_stitch, MIN_AREA_MM2
from worker import durable_json
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'mesh-repair'))
from analyze import load, topology, directed, features, nearest


def save(mesh,path):
    with path.open('w',encoding='utf-8') as stream:
        stream.write(f'OFF\n{len(mesh.vertices)} {len(mesh.faces)} 0\n')
        for p in mesh.vertices:
            stream.write(' '.join(format(x,'.17g') for x in p)+'\n')
        for f in mesh.faces:
            stream.write('3 '+' '.join(map(str,f))+'\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('config',type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding='utf-8'))
    output = Path(config['output'])
    original,mesh,source = (load(config[k]) for k in ['original','residual','authoritative'])
    lineage = json.loads(Path(config['lineage']).read_text(encoding='utf-8'))['input_face_plus_one']
    if len(lineage)!=len(mesh.faces):
        raise ValueError('Lineage length mismatch')
    ancestry = json.loads(Path(config['ancestry']).read_text(encoding='utf-8'))
    auth = supports(mesh,source)
    original_support = supports(mesh,original)
    original_authority = supports(original,source)
    generated = np.array(lineage)==0
    # A small repaired triangle happening to overlap LDraw is insufficient.
    # Its traced input triangle must also establish the same authoritative owner.
    retained = []
    for f,old_id in enumerate(lineage):
        retained.append(sorted(set(auth[f]) & set(original_authority[old_id-1]))
                        if old_id>0 and old_id-1 in original_support[f] else [])
    constrained,diagnostic = classify_and_stitch(mesh,source,ancestry,retained,generated)
    # Every face survives in order; seam joins affect vertex indices only.
    provenance = []
    for f,(old_id,owners) in enumerate(zip(lineage,auth)):
        kind = 'generated repair geometry' if old_id==0 else 'surviving input geometry'
        supported_lineage = old_id>0 and old_id-1 in original_support[f]
        authoritative = retained[f]
        provenance.append({'face':f,'kind':kind,'input_face':old_id-1 if old_id else None,
                           'input_lineage_geometrically_supported':supported_lineage,
                           'authoritative_geometric_support':owners,
                           'retained_authoritative_ancestry':[ancestry[a] for a in authoritative],
                           'fit_ownership':None,
                           'fit_reason':'No supported feature context proven; generated regions always unowned.'})
    for edge in diagnostic['boundary_edges']:
        f = edge['face']
        points = mesh.vertices[edge['vertices']]
        probes = np.vstack([points,points.mean(0)])
        _,d,ids = nearest(source,probes)
        edge.update(input_triangle_id=lineage[f]-1 if lineage[f] else None,
                    input_support_triangle_ids=original_support[f],
                    observational_authoritative_support=auth[f],
                    authoritative_nearest_triangle_ids=ids.tolist(),
                    authoritative_nearest_distances_mm=d.tolist())
    for path in diagnostic['loops']:
        edges = [diagnostic['boundary_edges'][i] for i in path['edges']]
        ids = sorted({a for e in edges for a in e['authoritative_nearest_triangle_ids']})
        path['nearby_authoritative_ancestry'] = [ancestry[a] for a in ids]
        path['input_triangle_ids'] = sorted({e['input_triangle_id'] for e in edges if e['input_triangle_id'] is not None})
        path['input_support_triangle_ids'] = sorted({a for e in edges for a in e['input_support_triangle_ids']})
        path['max_boundary_probe_distance_to_authoritative_mm'] = max(d for e in edges for d in e['authoritative_nearest_distances_mm'])
        path['intentional_opening_status'] = 'unproven; no capping permitted'
    save(constrained,output/'constrained.off')
    durable_json(output/'boundaries.json',diagnostic)
    durable_json(output/'provenance.json',{'faces':provenance,'new_generated_triangle_count':0,
        'inherited_generated_triangle_count':int(generated.sum()),
        'inherited_generated_area_mm2':float(mesh.area_faces[generated].sum()),
        'inherited_generated_area_percent':float(100*mesh.area_faces[generated].sum()/mesh.area)})
    attributed = np.array([bool(p['retained_authoritative_ancestry']) and len(p['retained_authoritative_ancestry'])==1 for p in provenance])
    primary = config['part']=='23422'
    report = {'part':config['part'],'residual':topology(mesh),'constrained':topology(constrained),
              'original':topology(original),'new_generated_triangle_count':0,
              'inherited_generated_triangle_count':int(generated.sum()),
              'inherited_generated_area_mm2':float(mesh.area_faces[generated].sum()),
              'authoritative_attributable_area_mm2':float(mesh.area_faces[attributed].sum()),
              'authoritative_attributable_area_percent':float(100*mesh.area_faces[attributed].sum()/mesh.area),
              'comparisons':{},'applied_seams':diagnostic['applied_seams']}
    count = 20000 if primary else 5000
    for name,target in [('authoritative',source),('original',original),('residual',mesh)]:
        if name=='residual' and np.array_equal(constrained.triangles,mesh.triangles):
            report['comparisons'][name] = {'identical_triangles':True,'max_mm':0,'p95_mm':0,'rms_mm':0}
        else:
            report['comparisons'][name] = {'repair_to':directed(constrained,target,count),'to_repair':directed(target,constrained,count)}
    if primary:
        bambu = load(config['bambu'])
        report['bambu'] = topology(bambu)
        report['comparisons']['bambu'] = {'repair_to':directed(constrained,bambu),'to_repair':directed(bambu,constrained)}
        report['features'] = {k:features(v) for k,v in [('original',original),('residual',mesh),('constrained',constrained),('bambu',bambu),('authoritative',source)]}
        # Verify both outer arm regions belong to the same face-adjacency component.
        components = trimesh.graph.connected_components(constrained.face_adjacency,nodes=np.arange(len(constrained.faces)),min_len=1)
        report['arms_share_component'] = any((constrained.triangles_center[c,0]<-6).any() and
            (constrained.triangles_center[c,0]>6).any() for c in components)
    report['acceptance'] = {'accepted':False,
        'one_component':report['constrained']['components']==1,
        'zero_boundary_edges':report['constrained']['boundary_edges']==0,
        'zero_nonmanifold_edges':report['constrained']['nonmanifold_edges']==0,
        'zero_near_degenerate_faces':bool(np.all(constrained.area_faces>=MIN_AREA_MM2)),
        'strict_bidirectional_source_fidelity_proven':False,
        'intentional_openings_and_global_wall_thickness_proven':False,
        'reason':'Independent exact intersection validation still required; generated/removed source regions and feature ownership are not certified.'}
    durable_json(output/'measurements.json',report)
    print(json.dumps({'part':config['part'],'applied_seams':diagnostic['applied_seams'],
                      'loop_count':len(diagnostic['loops']),'accepted':False}))


if __name__=='__main__':
    main()
