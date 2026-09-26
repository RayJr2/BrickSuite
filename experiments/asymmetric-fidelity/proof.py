"""Isolated conservative asymmetric-fidelity proof. No product acceptance changes."""
import json, csv, sys, math, time
from pathlib import Path
import numpy as np
import trimesh
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'mesh-repair'))
from analyze import nearest
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/asymmetric-fidelity-proof'
TOL=.20

def lattice(mesh,pitch=.5):
    points=[];ids=[]
    for index,t in enumerate(mesh.triangles):
        n=max(1,math.ceil(max(np.linalg.norm(t[a]-t[b]) for a,b in [(0,1),(1,2),(0,2)])/pitch))
        for i in range(n+1):
            for j in range(n-i+1):
                points.append(t[0]+(t[1]-t[0])*i/n+(t[2]-t[0])*j/n);ids.append(index)
    return np.array(points),np.array(ids)

def area_samples(mesh,pitch=.25):
    points=[];ids=[];weights=[]
    for index,t in enumerate(mesh.triangles):
        n=max(1,math.ceil(max(np.linalg.norm(t[a]-t[b]) for a,b in [(0,1),(1,2),(0,2)])/pitch))
        def p(i,j):return t[0]+(t[1]-t[0])*i/n+(t[2]-t[0])*j/n
        for i in range(n):
            for j in range(n-i):
                points.append((p(i,j)+p(i+1,j)+p(i,j+1))/3);ids.append(index);weights.append(mesh.area_faces[index]/n**2)
                if i+j<n-1:
                    points.append((p(i+1,j)+p(i+1,j+1)+p(i,j+1))/3);ids.append(index);weights.append(mesh.area_faces[index]/n**2)
    return np.array(points),np.array(ids),np.array(weights)

def winding(mesh,points):
    # Oriented solid angle sum in double precision, independently checked by ray parity.
    values=[]
    for p in points:
        a,b,c=np.moveaxis(mesh.triangles-p,1,0)
        la=np.linalg.norm(a,axis=1);lb=np.linalg.norm(b,axis=1);lc=np.linalg.norm(c,axis=1)
        numerator=np.einsum('ij,ij->i',a,np.cross(b,c))
        denominator=la*lb*lc+np.einsum('ij,ij->i',a,b)*lc+np.einsum('ij,ij->i',b,c)*la+np.einsum('ij,ij->i',c,a)*lb
        values.append(np.sum(2*np.arctan2(numerator,denominator))/(4*np.pi))
    return np.array(values)

def classify(mesh,points,valid_topology=True):
    _,distance,_=nearest(mesh,points)
    state=np.full(len(points),'near',dtype='<U16');far=np.flatnonzero(distance>TOL)
    if not len(far):return state,distance
    if not valid_topology:
        state[far]='untrusted';return state,distance
    q=points[far];w=winding(mesh,q);votes=[]
    for direction in ([1,.371,.129],[-.217,1,.493],[.317,-.281,1],[1,-.613,.719],[-.829,-.413,1]):
        d=np.array(direction);d=d/np.linalg.norm(d)
        hits,ray_ids,_=mesh.ray.intersects_location(q,np.tile(d,(len(q),1)),multiple_hits=True)
        counts=np.bincount(ray_ids,minlength=len(q));votes.append(counts%2)
    votes=np.array(votes)
    inside=(np.abs(w-1)<1e-7)&np.all(votes==1,axis=0)
    outside=(np.abs(w)<1e-7)&np.all(votes==0,axis=0)
    state[far]='ambiguous';state[far[inside]]='internal';state[far[outside]]='outside'
    return state,distance

def protected_faces(source,ancestry):
    # Deliberately conservative source-only guard. Never assign Blender ownership.
    protected=[];regions=[]
    for tri,record in zip(source.triangles,ancestry):
        paths=[r['file'] for r in record['references']]
        # 23422s01 explicitly authors INVERTNEXT radius-7 LDU cylinder at line 23.
        inner=any(r['file']=='p/2-4cylo.dat' and r['line']==23 for r in record['references'])
        if inner:region='authored_passage_wall'
        elif record['file']=='parts/23422.dat' or (record['file']=='parts/s/23422s01.dat' and record['line']>=90):region='authored_internal_spline'
        elif np.ptp(tri[:,2])<1e-6 and np.all(np.abs(tri[:,2])>3.99):region='opening_rim'
        elif any('cylo' in p for p in paths):region='outer_body_composition'
        else:region='arm_or_body_transition'
        regions.append(region);protected.append(region in ('authored_passage_wall','authored_internal_spline','opening_rim'))
    return np.array(protected),regions

def summarize(states,weights=None):
    return {str(k):{'count':int(np.sum(states==k)),**({'estimated_area_mm2':float(weights[states==k].sum())} if weights is not None else {})} for k in np.unique(states)}

def main():
    OUT.mkdir(exist_ok=True)
    base=ROOT/'build/blender-repair-proof'
    source=trimesh.load_mesh(base/'authoritative-source.off',process=False)
    old=trimesh.load_mesh(ROOT/'build/mesh-repair-proof/results/23422-source.off',process=False)
    assert np.array_equal(source.triangles,old.triangles),'Ancestry replay must match exactly'
    ancestry=json.loads((ROOT/'build/mesh-repair-proof/results/23422-ancestry.json').read_text())
    protected,regions=protected_faces(source,ancestry)
    points,ids=lattice(source);ap,ai,weights=area_samples(source)
    report={'threshold_mm':TOL,'source_area_mm2':float(source.area),'area_method':'equal-area microtriangle centroid quadrature, maximum edge subdivision pitch 0.25 mm; estimates, not exact partition','candidates':{}}
    for name,file,valid in [('blender','23422-blender-repaired.off',True),('bambu','23422-unvalidated-repair-source-mm.off',False)]:
        mesh=trimesh.load_mesh(base/file,process=False)
        mesh.vertices+=source.bounds.mean(0)-mesh.bounds.mean(0)
        states,dist=classify(mesh,points,valid)
        area_states,area_dist=classify(mesh,ap,valid)
        item={'samples':summarize(states),'area_quadrature':summarize(area_states,weights),'max_source_to_repair':float(dist.max()),'bounds':mesh.extents.tolist(),'protected_outliers':int(np.sum((dist>TOL)&protected[ids])),'regions':{}}
        for region in sorted(set(regions)):
            mask=np.array(regions)[ids]==region
            item['regions'][region]=summarize(states[mask])
        item['prototype_accepted']=bool(valid and not np.any(np.isin(states,['outside','ambiguous']))
            and not np.any(np.isin(area_states,['outside','ambiguous']))
            and not item['protected_outliers'] and not np.any((area_dist>TOL)&protected[ai]))
        if valid:
            rp,_=lattice(mesh);_,rd,_=nearest(source,rp)
            item['max_repair_to_source']=float(rd.max());item['prototype_accepted'] &= bool(rd.max()<=TOL)
        report['candidates'][name]=item
        with (OUT/(name+'-outliers.csv')).open('w',newline='') as f:
            writer=csv.writer(f);writer.writerow(['triangle','x','y','z','distance_mm','classification','protected','region','source_file','source_line','ancestry'])
            for p,i,d,s in zip(points,ids,dist,states):
                if d>TOL:writer.writerow([i,*p,d,s,bool(protected[i]),regions[i],ancestry[i]['file'],ancestry[i]['line'],json.dumps(ancestry[i]['references'])])
        if name=='blender':
            import matplotlib;matplotlib.use('Agg')
            import matplotlib.pyplot as plt
            fig,axes=plt.subplots(1,2,figsize=(13,5))
            for ax,coords,label in zip(axes,[(0,1),(0,2)],['Top: X/Y','Side: X/Z']):
                ax.scatter(points[:,coords[0]],points[:,coords[1]],s=1,c='#cccccc')
                for state,color in [('internal','#1676b5'),('outside','#d62728'),('ambiguous','#b060c0')]:
                    m=states==state;ax.scatter(points[m,coords[0]],points[m,coords[1]],s=8,c=color,label=state)
                m=(dist>TOL)&protected[ids];ax.scatter(points[m,coords[0]],points[m,coords[1]],s=20,facecolors='none',edgecolors='#ff9800',label='protected outlier')
                ax.set_aspect('equal');ax.set_title(label+' (mm)');ax.legend(fontsize=8)
            fig.suptitle('23422 source samples: internal does not imply safe to remove');fig.tight_layout();fig.savefig(OUT/'outliers.png',dpi=160);plt.close(fig)
        (OUT/'report.json').write_text(json.dumps(report,indent=2))
        print(name,json.dumps(item),flush=True)

if __name__=='__main__':main()
