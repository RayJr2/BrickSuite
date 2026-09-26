"""Conservative residual seam proof. No proximity-only joins or automatic caps."""
import json
import numpy as np
import trimesh

SOURCE_EPS_MM = .0004
NUMERIC_SEAM_EPS_MM = 1e-9
MIN_AREA_MM2 = 1e-12


def supports(mesh, source):
    """Supporting input triangles; does not grant semantic or causal ownership."""
    answer = []
    tree = source.triangles_tree
    normal_cos = np.cos(np.deg2rad(.1))
    for i,t in enumerate(mesh.triangles):
        possible = np.array(list(tree.intersection(np.r_[t.min(0)-SOURCE_EPS_MM,t.max(0)+SOURCE_EPS_MM])),dtype=int)
        if not len(possible) or mesh.area_faces[i] < MIN_AREA_MM2:
            answer.append([])
            continue
        distances = np.zeros(len(possible))
        for p in np.vstack([t,t.mean(0)]):
            q = trimesh.triangles.closest_point(source.triangles[possible],np.tile(p,(len(possible),1)))
            distances = np.maximum(distances,np.linalg.norm(q-p,axis=1))
        normals = np.abs(source.face_normals[possible]@mesh.face_normals[i]) >= normal_cos
        answer.append(possible[(distances<=SOURCE_EPS_MM)&normals].tolist())
    return answer


def boundary(mesh):
    counts = np.bincount(mesh.edges_unique_inverse)
    wanted = set(np.flatnonzero(counts==1).tolist())
    # Directed edges retain winding and their incident face.
    return [{'vertices':edge.tolist(),'face':int(face)}
            for edge,face,uid in zip(mesh.edges,mesh.edges_face,mesh.edges_unique_inverse) if uid in wanted]


def boundary_paths(mesh, edges):
    if not edges:
        return []
    indexed = np.array([e['vertices'] for e in edges])
    groups = trimesh.graph.connected_components(indexed,nodes=np.unique(indexed),min_len=1)
    paths = []
    for vertices in groups:
        ids = np.flatnonzero(np.isin(indexed[:,0],vertices))
        degree = np.bincount(indexed[ids].ravel(),minlength=len(mesh.vertices))[vertices]
        points = mesh.vertices[vertices]
        paths.append({'edges':ids.tolist(),'vertices':vertices.tolist(),
                      'closed_simple_graph':bool(np.all(degree==2)),
                      'bounds_mm':[points.min(0).tolist(),points.max(0).tolist()],
                      'length_mm':float(np.linalg.norm(mesh.vertices[indexed[ids,0]]-mesh.vertices[indexed[ids,1]],axis=1).sum())})
    return paths


def exact_authored_edge(source, a, b):
    if a == b:
        return None
    common = sorted(set(map(tuple,source.triangles[a])) & set(map(tuple,source.triangles[b])))
    return np.array(common) if len(common)==2 else None


def on_segment(points, segment):
    delta = segment[1]-segment[0]
    squared = float(delta@delta)
    if squared == 0:
        return False
    t = (points-segment[0])@delta/squared
    projected = segment[0]+t[:,None]*delta
    return bool(np.all(t>=-1e-10) and np.all(t<=1+1e-10) and
                np.all(np.linalg.norm(projected-points,axis=1)<=NUMERIC_SEAM_EPS_MM))


def source_edge_incidence(source):
    incidence = {}
    for f,t in enumerate(source.triangles):
        for a,b in [(0,1),(1,2),(2,0)]:
            key = tuple(sorted([tuple(t[a]),tuple(t[b])]))
            incidence.setdefault(key,[]).append(f)
    return incidence


def seam_proof(mesh, edge_a, edge_b, source, owners, generated, incidence):
    fa,fb = edge_a['face'],edge_b['face']
    if fa == fb or generated[fa] or generated[fb]:
        return None
    if len(owners[fa])!=1 or len(owners[fb])!=1:
        return None
    authored = exact_authored_edge(source,owners[fa][0],owners[fb][0])
    if authored is None:
        return None
    # Coincident boundaries of separate touching solids are not a seam proof.
    # The authoritative edge must have precisely these two incident triangles.
    key = tuple(sorted(map(tuple,authored)))
    if sorted(incidence.get(key,[]))!=sorted([owners[fa][0],owners[fb][0]]):
        return None
    a = mesh.vertices[edge_a['vertices']]
    b = mesh.vertices[edge_b['vertices']]
    if np.max(np.linalg.norm(a-b[::-1],axis=1)) > NUMERIC_SEAM_EPS_MM:
        return None
    if not on_segment(np.vstack([a,b]),authored):
        return None
    return {'source_triangles':[owners[fa][0],owners[fb][0]],
            'authored_shared_edge_mm':authored.tolist(),
            'reason':'distinct source faces share this exact authored edge; reversed coincident residual intervals'}


def classify_and_stitch(mesh, source, ancestry, source_support, generated, protected_edges=()):
    edges = boundary(mesh)
    paths = boundary_paths(mesh,edges)
    eligible = {}
    nearby = {i:[] for i in range(len(edges))}
    protected = set(protected_edges)
    incidence = source_edge_incidence(source)
    for i,a in enumerate(edges):
        for j in range(i+1,len(edges)):
            b = edges[j]
            pa,pb = mesh.vertices[a['vertices']],mesh.vertices[b['vertices']]
            distance = min(np.max(np.linalg.norm(pa-pb,axis=1)),np.max(np.linalg.norm(pa-pb[::-1],axis=1)))
            if distance<=SOURCE_EPS_MM:
                nearby[i].append({'edge':j,'endpoint_error_mm':float(distance)})
                nearby[j].append({'edge':i,'endpoint_error_mm':float(distance)})
            if i in protected or j in protected:
                continue
            proof = seam_proof(mesh,a,b,source,source_support,generated,incidence)
            if proof is not None:
                eligible.setdefault(i,[]).append((j,proof))
                eligible.setdefault(j,[]).append((i,proof))
    accepted = []
    for i,candidates in eligible.items():
        if len(candidates)!=1:
            continue
        j,proof = candidates[0]
        if i<j and len(eligible.get(j,[]))==1:
            accepted.append({'edges':[i,j],**proof})
    proved_edges = {e for p in accepted for e in p['edges']}
    for i,path in enumerate(paths):
        ids = path['edges']
        faces = sorted({edges[e]['face'] for e in ids})
        supported = [e for e in ids if len(source_support[edges[e]['face']])==1 and not generated[edges[e]['face']]]
        if all(e in protected for e in ids):
            classification = 'intentional opening'
        elif all(e in proved_edges for e in ids):
            classification = 'stitchable authored seam'
        else:
            classification = 'ambiguous/reject'
        owners = sorted({o for f in faces for o in source_support[f]})
        path.update(loop=i,classification=classification,incident_faces=faces,
                    authoritative_supported_edges=len(supported),proved_seam_edges=sum(e in proved_edges for e in ids),
                    source_triangle_ids=owners,source_ancestry=[ancestry[o] for o in owners],
                    generated_incident_faces=[f for f in faces if generated[f]],
                    patch_gate={'complete_authoritative_boundary':len(supported)==len(ids),
                                'intentional_opening_excluded':False,'material_side_proven':False,
                                'patch_fidelity_proven':False,'wall_and_feature_topology_proven':False},
                    patch_allowed=False,
                    reason='No patch is authorized without every local material/opening/fidelity proof; graph closure alone is insufficient.')
    for i,e in enumerate(edges):
        e.update(edge=i,nearby_opposing_or_coincident_edges=nearby[i],
                 authoritative_triangle_ids=source_support[e['face']])
    vertices = mesh.vertices.copy()
    parent = np.arange(len(vertices))
    def root(v):
        while parent[v]!=v:
            v = parent[v]
        return v
    for pair in accepted:
        a,b = (edges[e]['vertices'] for e in pair['edges'])
        for x,y in zip(a,b[::-1]):
            rx,ry = root(x),root(y)
            parent[max(rx,ry)] = min(rx,ry)
    mapping = np.array([root(i) for i in range(len(vertices))])
    candidate = trimesh.Trimesh(vertices=vertices,faces=mapping[mesh.faces],process=False)
    counts = np.bincount(candidate.edges_unique_inverse)
    collapsed = np.any(np.diff(np.sort(candidate.faces,axis=1),axis=1)==0,axis=1)
    newly_small = (candidate.area_faces<MIN_AREA_MM2)&(mesh.area_faces>=MIN_AREA_MM2)
    rollback = bool((counts>2).any() or collapsed.any() or newly_small.any())
    if rollback:
        candidate = mesh.copy()
    else:
        candidate.remove_unreferenced_vertices()
    return candidate, {'loops':paths,'boundary_edges':edges,'seam_proposals':accepted,
                       'rolled_back':rollback,'applied_seams':0 if rollback else len(accepted),
                       'generated_repair_triangles_added':0,'generated_repair_area_added_mm2':0,
                       'provenance_rule':'existing per-face lineage is preserved; no generated face gets fit ownership'}
