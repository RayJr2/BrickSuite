"""Deterministic sampled surface distances, conservative geometric ancestry, and sections.

Distances are to triangles, not to vertices. Reported maxima are sampled lower
bounds on Hausdorff distance, not a certified global maximum. No fit acceptance
tolerance is inferred from these measurements.
"""
import argparse
import itertools
import json
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree
import trimesh
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

OWNERSHIP_EPS_MM = .0004  # Existing seam tolerance, not an acceptance concession.


def load(path):
    return trimesh.load_mesh(path, process=False)


def samples(mesh, random_count=20000, subdivisions=4):
    bary = np.array([[i/subdivisions, j/subdivisions, 1-(i+j)/subdivisions]
                     for i in range(subdivisions+1) for j in range(subdivisions+1-i)])
    grid = np.einsum('ka,fad->fkd', bary, mesh.triangles).reshape(-1,3)
    rng = np.random.default_rng(37023422)
    ids = rng.choice(len(mesh.faces), random_count, p=mesh.area_faces/mesh.area)
    uv = rng.random((random_count,2)); uv[uv.sum(axis=1)>1] = 1-uv[uv.sum(axis=1)>1]
    tri = mesh.triangles[ids]
    pts = tri[:,0]+uv[:,0,None]*(tri[:,1]-tri[:,0])+uv[:,1,None]*(tri[:,2]-tri[:,0])
    return grid, pts


def nearest(mesh, points):
    locations=[]; distances=[]; faces=[]
    for start in range(0,len(points),256):
        q,d,f=trimesh.proximity.closest_point(mesh,points[start:start+256])
        locations.append(q);distances.append(d);faces.append(f)
    return np.vstack(locations),np.concatenate(distances),np.concatenate(faces)


def directed(a,b,random_count=20000):
    grid,area=samples(a,random_count)
    points=np.vstack([grid,area]);q,d,f=nearest(b,points)
    worst=np.argsort(d)[-5:][::-1]
    area_d=d[len(grid):]
    return {'sample_count':len(d),'sampled_max_mm':float(d.max()),
        'area_sample_p95_mm':float(np.percentile(area_d,95)),
        'area_sample_p99_mm':float(np.percentile(area_d,99)),
        'area_sample_rms_mm':float(np.sqrt(np.mean(area_d**2))),
        'region_sampled_max_mm':{name:float(d[mask].max()) if mask.any() else None for name,mask in
            [('left_arm',points[:,0]<-4),('body',np.abs(points[:,0])<=4),('right_arm',points[:,0]>4)]},
        'worst':[{'point_mm':points[i].tolist(),'nearest_mm':q[i].tolist(),
                  'target_face':int(f[i]),'distance_mm':float(d[i])} for i in worst]}


def topology(mesh):
    counts=np.bincount(mesh.edges_unique_inverse)
    adjacency=mesh.face_adjacency
    components=trimesh.graph.connected_components(adjacency,nodes=np.arange(len(mesh.faces)),min_len=1)
    return {'vertices':len(mesh.vertices),'faces':len(mesh.faces),'components':len(components),
        'component_faces':sorted([len(c) for c in components],reverse=True),
        'boundary_edges':int((counts==1).sum()),'nonmanifold_edges':int((counts>2).sum()),
        'degenerate_faces_area_below_1e-12_mm2':int((mesh.area_faces<1e-12).sum()),
        'watertight':bool(mesh.is_watertight),'winding_consistent':bool(mesh.is_winding_consistent),
        'euler_number':int(mesh.euler_number),'bounds_mm':mesh.bounds.tolist(),
        'dimensions_mm':mesh.extents.tolist(),'area_mm2':float(mesh.area)}


def ownership(candidate,source,ancestry):
    counts={'unique_triangle':0,'ancestry_only':0,'ambiguous':0,'new_or_unmatched':0}
    areas={k:0. for k in counts}; tree=source.triangles_tree
    normal_cos=np.cos(np.deg2rad(.1))
    for i,t in enumerate(candidate.triangles):
        candidates=np.array(list(tree.intersection(np.r_[t.min(0)-OWNERSHIP_EPS_MM,t.max(0)+OWNERSHIP_EPS_MM])),dtype=int)
        valid=[]
        if len(candidates):
            probes=np.vstack([t,t.mean(0)])
            d=np.zeros(len(candidates))
            for point in probes:
                closest=trimesh.triangles.closest_point(source.triangles[candidates],np.tile(point,(len(candidates),1)))
                d=np.maximum(d,np.linalg.norm(closest-point,axis=1))
            aligned=np.abs(source.face_normals[candidates]@candidate.face_normals[i])>=normal_cos
            valid=candidates[(d<=OWNERSHIP_EPS_MM)&aligned]
        if len(valid)==1:key='unique_triangle'
        elif len(valid)>1:
            identities={(ancestry[j]['file'],ancestry[j]['line'],json.dumps(ancestry[j]['references'],sort_keys=True)) for j in valid}
            key='ancestry_only' if len(identities)==1 else 'ambiguous'
        else:key='new_or_unmatched'
        counts[key]+=1;areas[key]+=candidate.area_faces[i]
    return {'distance_epsilon_mm':OWNERSHIP_EPS_MM,'normal_angle_degrees':.1,
            'face_counts':counts,'face_percent':{k:100*v/len(candidate.faces) for k,v in counts.items()},
            'area_percent':{k:100*v/candidate.area for k,v in areas.items()}}


def rays(mesh,origins,directions):
    locations, ray_ids, _=mesh.ray.intersects_location(origins,directions,multiple_hits=True)
    locations=np.asarray(locations).reshape((-1,3))
    return [np.unique(np.round(((locations[ray_ids==i]-origins[i])@directions[i]),7)).tolist() for i in range(len(origins))]


def features(mesh):
    # Canonical mm: LDraw (x,y,z) -> (.4x,.4z,-.4y). Bore axis is Z.
    origins=np.array([[x,y,-10.] for x,y in [(0,0),(1,0),(-1,0),(0,1),(0,-1)]])
    bore=rays(mesh,origins,np.tile([0.,0.,1.],(len(origins),1)))
    angles=np.arange(0,360,30)*np.pi/180
    directions=np.array([[np.cos(a),np.sin(a),0] for a in angles])
    radial=rays(mesh,np.tile([0.,0.,2.5],(len(angles),1)),directions)
    return {'bore_axial_probe_hits':bore,'body_radial_hits_at_z_2_5_mm':radial,
            'radial_wall_mm':[r[-1]-r[0] if len(r)>=2 else None for r in radial]}


def align(reference,source):
    # Test only proper axis permutations, translation, and the separately declared
    # exact LDU/mm factor. No deformable registration or fitted scale.
    scaled=reference.vertices*.4
    tree=cKDTree(source.vertices)
    choices=[]
    for perm in itertools.permutations(range(3)):
        for signs in itertools.product([-1,1],repeat=3):
            rotation=np.eye(3)[list(perm)]*np.array(signs)[:,None]
            if np.linalg.det(rotation)<0:continue
            v=scaled@rotation.T
            translation=source.bounds.mean(0)-(v.min(0)+v.max(0))/2
            v+=translation
            score=float(np.mean(tree.query(v)[0]**2))
            choices.append((score,rotation,translation,v))
    score,rotation,translation,vertices=min(choices,key=lambda item:item[0])
    aligned=trimesh.Trimesh(vertices=vertices,faces=reference.faces,process=False)
    return aligned,{'scale':.4,'rotation':rotation.tolist(),'translation_mm':translation.tolist(),
                    'vertex_registration_score_mm2':score,'scale_fitted':False}


def plot(meshes,path):
    figure=plt.figure(figsize=(16,9))
    for i,(name,mesh) in enumerate(meshes.items()):
        axis=figure.add_subplot(2,3,i+1,projection='3d')
        axis.add_collection3d(Poly3DCollection(mesh.triangles,facecolor='#78a9c5',edgecolor='#293943',linewidth=.15,alpha=.85))
        axis.set(xlim=(-14,14),ylim=(-5,5),zlim=(-5,5),xlabel='X mm',ylabel='Y mm',zlabel='Z mm',title=name)
        axis.set_box_aspect((28,10,10));axis.view_init(elev=55,azim=-80)
    figure.tight_layout();figure.savefig(path,dpi=160);plt.close(figure)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);args=parser.parse_args();root=args.directory
    report={'method':'fixed-seed area sampling plus barycentric grid; sampled maxima, not certified Hausdorff bounds','parts':{}}
    for part in ['23422','44375a','80910','6553','3001']:
        source=load(root/f'{part}-source.off');ancestry=json.loads((root/f'{part}-ancestry.json').read_text())
        result={'source':topology(source),'variants':{}}
        if part=='23422':result['source_feature_probes']=features(source)
        for mode in ['candidate','clean','refine','fill','fill-clean','local']:
            path=root/f'{part}-{mode}.off'
            if not path.exists():continue
            mesh=load(path)
            if not len(mesh.faces):continue
            item={'topology':topology(mesh)}
            if part=='23422' or mode in ['local','fill-clean']:
                item['to_source']=directed(mesh,source,20000 if part=='23422' else 5000)
                item['source_to']=directed(source,mesh,20000 if part=='23422' else 5000)
                item['ownership']=ownership(mesh,source,ancestry)
            if part=='23422':item['feature_probes']=features(mesh)
            result['variants'][mode]=item
            print(part,mode,'analyzed',flush=True)
        report['parts'][part]=result
        (root/'analysis.json').write_text(json.dumps(report,indent=2))
    source=load(root/'23422-source.off');bambu=load(root/'23422-bambu-raw.off')
    aligned,registration=align(bambu,source);aligned.export(root/'23422-bambu-normalized.off')
    raw_centered=bambu.copy();raw_centered.vertices=bambu.vertices@np.array(registration['rotation']).T
    raw_centered.vertices+=source.bounds.mean(0)-raw_centered.bounds.mean(0)
    report['bambu']={'raw':topology(bambu),'registration':registration,'normalized':topology(aligned),
        'to_source':directed(aligned,source),'source_to':directed(source,aligned),
        'ownership':ownership(aligned,source,json.loads((root/'23422-ancestry.json').read_text())),
        'feature_probes':features(aligned),'candidate_comparisons':{},
        'raw_scale_to_source':directed(raw_centered,source),
        'source_to_raw_scale':directed(source,raw_centered)}
    for mode in ['fill','fill-clean','local']:
        path=root/f'23422-{mode}.off'
        if path.exists():
            candidate=load(path)
            report['bambu']['candidate_comparisons'][mode]={'candidate_to_bambu':directed(candidate,aligned),'bambu_to_candidate':directed(aligned,candidate)}
    (root/'analysis.json').write_text(json.dumps(report,indent=2))
    views={'LDraw triangulated source':source,'Unrepaired source candidate':load(root/'23422-candidate.off'),
           'Bambu reference (0.4x normalized)':aligned,'CGAL refine + fill (rejected)':load(root/'23422-fill.off')}
    for mode in ['fill-clean','local']:
        if (root/f'23422-{mode}.off').exists():views['CGAL '+mode]=load(root/f'23422-{mode}.off')
    plot(views,root/'23422-comparison.png')
    figure,axes=plt.subplots(2,3,figsize=(15,8))
    for col,(title,mesh) in enumerate([('Bambu (0.4x)',aligned),('CGAL refine + fill',load(root/'23422-fill.off')),('CGAL local',load(root/'23422-local.off'))]):
        for row,z in enumerate([0.123,2.5]):
            axis=axes[row,col]
            for model,color,label in [(source,'#777777','LDraw'),(mesh,'#d34d28',title)]:
                lines=trimesh.intersections.mesh_plane(model,[0,0,1],[0,0,z])
                for j,line in enumerate(lines):axis.plot(line[:,0],line[:,1],color=color,linewidth=1,label=label if j==0 else None)
            axis.set_aspect('equal');axis.set_title(f'{title}: Z={z} mm');axis.set_xlabel('X mm');axis.set_ylabel('Y mm');axis.legend()
    figure.tight_layout();figure.savefig(root/'23422-sections.png',dpi=160);plt.close(figure)


if __name__=='__main__':main()
