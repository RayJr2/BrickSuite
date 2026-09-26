"""Feature probes and fixed-seed sampled secondary comparison; all coordinates mm."""
import json, sys
from pathlib import Path
import numpy as np
import trimesh
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'mesh-repair'))
from analyze import features, nearest, rays
root=Path('C:/Programming/Qt/BrickSuite/build/blender-repair-proof')
meshes={name:trimesh.load_mesh(root/file,process=False) for name,file in {
    'source':'authoritative-source.off','blender':'23422-blender-repaired.off',
    'bambu':'23422-unvalidated-repair-source-mm.off'}.items()}
# Same translation-only centering as the existing validator; no rotation or scaling.
meshes['bambu'].vertices+=meshes['source'].bounds.mean(0)-meshes['bambu'].bounds.mean(0)
report={'features':{name:features(mesh) for name,mesh in meshes.items()},
        'bounds':{name:mesh.extents.tolist() for name,mesh in meshes.items()},'comparisons':{}}
angles=np.arange(360)*np.pi/180
directions=np.array([[np.cos(a),np.sin(a),0] for a in angles])
for name,mesh in meshes.items():
    hits=rays(mesh,np.zeros((360,3)),directions)
    first=[h[0] for h in hits if h]
    report['features'][name]['midplane_minimum_radial_clearance_diameter']=2*min(first)
    report['features'][name]['midplane_missing_ray_count']=sum(not h for h in hits)
for a,b in [('blender','bambu'),('bambu','blender')]:
    mesh=meshes[a];rng=np.random.default_rng(23422)
    ids=rng.choice(len(mesh.faces),5000,p=mesh.area_faces/mesh.area)
    uv=rng.random((5000,2));uv[uv.sum(axis=1)>1]=1-uv[uv.sum(axis=1)>1]
    t=mesh.triangles[ids];p=t[:,0]+uv[:,0,None]*(t[:,1]-t[:,0])+uv[:,1,None]*(t[:,2]-t[:,0])
    q,d,f=nearest(meshes[b],p);i=int(np.argmax(d))
    report['comparisons'][a+'_to_'+b]={'max':float(d.max()),'p95':float(np.percentile(d,95)),
        'rms':float(np.sqrt(np.mean(d*d))),'worst_point':p[i].tolist(),'samples':5000}
(root/'comparison.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
