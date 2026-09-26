"""Source-ancestry labeling and opening probes, reusing saved containment evidence."""
import csv,json,sys
from pathlib import Path
import numpy as np
import trimesh
from proof import ROOT,OUT,lattice,protected_faces,summarize
sys.path.insert(0,str(ROOT/'experiments/mesh-repair'))
from analyze import rays
source=trimesh.load_mesh(ROOT/'build/blender-repair-proof/authoritative-source.off',process=False)
repair=trimesh.load_mesh(ROOT/'build/blender-repair-proof/23422-blender-repaired.off',process=False)
repair.vertices+=source.bounds.mean(0)-repair.bounds.mean(0)
ancestry=json.loads((ROOT/'build/mesh-repair-proof/results/23422-ancestry.json').read_text())
protected,regions=protected_faces(source,ancestry)
p,ids=lattice(source)
report=json.loads((OUT/'report.json').read_text())
for name in ('blender','bambu'):
    path=OUT/(name+'-outliers.csv');rows=list(csv.DictReader(path.open()))
    item=report['candidates'][name];item['regions']={}
    for region in sorted(set(regions)):
        total=int(np.sum(np.array(regions)[ids]==region));sub=[r for r in rows if regions[int(r['triangle'])]==region]
        item['regions'][region]={'near':{'count':total-len(sub)}}
        for state in sorted(set(r['classification'] for r in sub)):
            item['regions'][region][state]={'count':sum(r['classification']==state for r in sub)}
    for row in rows:
        i=int(row['triangle']);row['protected']=bool(protected[i]);row['region']=regions[i]
    item['protected_outliers']=sum(protected[int(r['triangle'])] for r in rows).item()
    with path.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
    if name=='blender':blender_rows=rows

# A radial occlusion witness is diagnostic, NOT a general semantic exposure proof.
wall=[r for r in blender_rows if r['region']=='authored_passage_wall']
q=np.array([[float(r[k]) for k in ('x','y','z')] for r in wall])
origins=q.copy();origins[:,:2]=0;directions=q-origins;radius=np.linalg.norm(directions,axis=1);directions/=radius[:,None]
hits=rays(source,origins,directions)
report['passage_outlier_radial_occlusion_witnesses']=int(sum(bool(h) and h[0]<d-.2 for h,d in zip(hits,radius)))
report['opening_probes']={}
for name,mesh in [('source',source),('blender',repair)]:
    angles=np.arange(360)*np.pi/180
    d=np.array([[np.cos(a),np.sin(a),0] for a in angles]);sections={}
    for z in (0.,2.5):
        h=rays(mesh,np.tile([0,0,z],(360,1)),d)
        sections[str(z)]={'minimum_radial_clearance_diameter_mm':2*min(v[0] for v in h if v),'cardinal_inner_diameter_x_mm':h[0][0]+h[180][0],'cardinal_outer_diameter_y_mm':h[90][-1]+h[270][-1]}
    xy=np.array([(x,y) for x in np.arange(-1.8,1.801,.1) for y in np.arange(-1.8,1.801,.1) if x*x+y*y<=1.8**2])
    h=rays(mesh,np.column_stack((xy,np.full(len(xy),-5))),np.tile([0,0,1],(len(xy),1)))
    report['opening_probes'][name]={'axial_clear_rays':sum(not v for v in h),'axial_test_rays':len(h),'sections':sections}

import matplotlib;matplotlib.use('Agg')
import matplotlib.pyplot as plt
fig,axes=plt.subplots(1,2,figsize=(13,5))
q=np.array([[float(r[k]) for k in ('x','y','z')] for r in blender_rows])
for ax,(a,b),title in zip(axes,[(0,1),(0,2)],['Top X/Y','Side X/Z']):
    ax.scatter(p[:,a],p[:,b],s=1,c='#dddddd')
    for region,color in [('outer_body_composition','#1378b5'),('arm_or_body_transition','#279e68'),('authored_passage_wall','#e07a11')]:
        m=np.array([r['region']==region for r in blender_rows]);ax.scatter(q[m,a],q[m,b],s=8,c=color,label=region)
    ax.set_aspect('equal');ax.set_title(title+' (mm)');ax.legend(fontsize=7)
fig.suptitle('All distant samples are inside; passage-wall exposure remains a separate question')
fig.tight_layout();fig.savefig(OUT/'outliers.png',dpi=160);plt.close(fig)
(OUT/'report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
