"""Bounded source-only exposure certificate; unresolved occlusion fails closed."""
from fractions import Fraction
import numpy as np

def hemisphere(normal):
    axis=np.array([1.,0,0]) if abs(normal[0])<.8 else np.array([0.,1,0])
    u=np.cross(normal,axis);u/=np.linalg.norm(u);v=np.cross(normal,u)
    directions=[normal]
    for cosine in (.95,.75,.5,.25):
        for angle in np.arange(16)*2*np.pi/16:
            directions.append(normal*cosine+(u*np.cos(angle)+v*np.sin(angle))*np.sqrt(1-cosine*cosine))
    return np.array(directions)

def first_hits(triangles,origins,directions):
    # Double precision conservative triangle hits: include edge hits as blockers.
    e1=triangles[:,1]-triangles[:,0];e2=triangles[:,2]-triangles[:,0]
    h=np.cross(directions[:,None,:],e2[None,:,:]);det=np.einsum('fi,dfi->df',e1,h)
    good=np.abs(det)>1e-12;inv=np.divide(1.,det,out=np.zeros_like(det),where=good)
    s=origins[:,None,:]-triangles[None,:,0,:]
    u=np.einsum('dfi,dfi->df',s,h)*inv;q=np.cross(s,e1[None,:,:])
    v=np.einsum('di,dfi->df',directions,q)*inv;t=np.einsum('fi,dfi->df',e2,q)*inv
    valid=good&(u>=-1e-9)&(v>=-1e-9)&(u+v<=1+1e-9)&(t>1e-8)
    distance=np.where(valid,t,np.inf);ids=distance.argmin(axis=1)
    return distance[np.arange(len(directions)),ids],ids

def exact_plane_sign(triangle,point):
    a,b,c=[[Fraction(float(x)) for x in p] for p in triangle]
    q=[Fraction(float(x))-a[i] for i,x in enumerate(point)]
    u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
    normal=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
    return sum(normal[i]*q[i] for i in range(3))

def convex_source_cells(mesh):
    # No invented caps or proximity welding. Only closed authored components qualify.
    cells=[]
    if len(mesh.faces)>2000:raise ValueError('Source proof exceeds 2,000-face workload limit')
    for component in mesh.split(only_watertight=False,repair=False):
        if not component.is_watertight or not component.is_winding_consistent or component.volume<=0:continue
        if not component.is_convex:continue
        normals=component.face_normals;triangles=component.triangles
        # Exact signs establish all source vertices in the oriented convex halfspaces.
        if any(exact_plane_sign(t,p)>0 for t in triangles for p in component.vertices):continue
        cells.append(component)
    return cells

def certify(mesh,point,normal,protected=False,cells=()):
    # The repair mesh is deliberately absent from this API.
    if not protected:
        for cell in cells:
            signed=np.einsum('fi,fi->f',cell.face_normals,point-cell.triangles[:,0])
            if np.all(signed < -1e-5) and all(exact_plane_sign(t,point)<0 for t in cell.triangles):
                return {'classification':'certified_internal','basis':'closed authoritative convex component; strict exact halfspace signs'}
    directions=hemisphere(normal);evidence=[];normal_blocker=None;normal_distance=None
    for offset in (1e-6,1e-5):
        origins=np.tile(point+normal*offset,(len(directions),1))
        distance,ids=first_hits(mesh.triangles,origins,directions)
        if np.isfinite(distance[0]):normal_blocker=int(ids[0]);normal_distance=float(distance[0])
        evidence.append(np.isinf(distance))
    stable=evidence[0]&evidence[1]
    # Strengthen escape witness using a finite angular neighborhood, not one grazing ray.
    for index in np.flatnonzero(stable):
        d=directions[index];axis=np.array([1.,0,0]) if abs(d[0])<.8 else np.array([0.,1,0])
        u=np.cross(d,axis);u/=np.linalg.norm(u);v=np.cross(d,u)
        probes=np.array([d+.005*t for t in (u,-u,v,-v)]);probes/=np.linalg.norm(probes,axis=1)[:,None]
        clear=True
        for offset in (1e-6,1e-5):
            distance,_=first_hits(mesh.triangles,np.tile(point+normal*offset,(4,1)),probes)
            clear &= bool(np.all(np.isinf(distance)))
        if clear:
            return {'classification':'protected_exposed' if protected else 'required_exterior',
                    'stable_escape_directions':int(stable.sum()),'escape_direction':d.tolist(),
                    'basis':'source-only outward hemisphere escape with offset and angular perturbation checks'}
    return {'classification':'ambiguous','stable_escape_directions':int(stable.sum()),
            'normal_blocker_triangle':normal_blocker,'normal_blocker_distance_mm':normal_distance,
            'basis':'finite blocked rays or grazing escape do not prove complete occlusion; no admissible closed source cell'}
