"""Headless Blender proof. Operates on copies of captured world-space geometry."""
import bpy, bmesh, json, struct, time
from pathlib import Path
from mathutils.bvhtree import BVHTree
OUT=Path('C:/Programming/Qt/BrickSuite/build/blender-repair-proof')
source=json.loads((OUT/'original-scene-meshes.json').read_text())

def make(name, vertices, faces):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices, [], faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    return obj

def stats(obj):
    bm=bmesh.new();bm.from_mesh(obj.data);bm.verts.ensure_lookup_table();bm.faces.ensure_lookup_table()
    remaining=set(bm.faces);components=[]
    while remaining:
        todo=[remaining.pop()];group=[]
        while todo:
            face=todo.pop();group.append(face)
            for edge in face.edges:
                for neighbor in edge.link_faces:
                    if neighbor in remaining:remaining.remove(neighbor);todo.append(neighbor)
        components.append(len(group))
    vs=[v.co for v in bm.verts]
    result=dict(vertices=len(vs),faces=len(bm.faces),components=components,
        boundary_edges=sum(e.is_boundary for e in bm.edges),
        nonmanifold_edges=sum(len(e.link_faces)>2 for e in bm.edges),
        nonmanifold_vertices=sum(not v.is_manifold for v in bm.verts),
        degenerates=sum(f.calc_area()<1e-12 for f in bm.faces),
        bounds=[max(v[i] for v in vs)-min(v[i] for v in vs) for i in range(3)],
        boundary_segments=[[list(v.co) for v in e.verts] for e in bm.edges if e.is_boundary])
    bm.free();return result

def export(obj,name):
    obj.data.calc_loop_triangles()
    with (OUT/name).open('wb') as f:
        f.write(b'BrickSuite Blender experiment - raw millimeter coordinates'.ljust(80,b'\0'))
        f.write(struct.pack('<I',len(obj.data.loop_triangles)))
        for tri in obj.data.loop_triangles:
            coords=[obj.data.vertices[i].co for i in tri.vertices]
            f.write(struct.pack('<12fH',*tri.normal,*coords[0],*coords[1],*coords[2],0))

objects=[make(o['name']+'-proof',o['vertices'],o['faces']) for o in source['objects']]
for i,obj in enumerate(objects):
    export(obj,f'component-{i}.stl')
    for j in range(i):
        other=objects[j];v=[list(p.co) for p in obj.data.vertices]+[list(p.co) for p in other.data.vertices]
        f=[list(p.vertices) for p in obj.data.polygons]+[[k+len(obj.data.vertices) for k in p.vertices] for p in other.data.polygons]
        export(make(f'pair-{j}-{i}',v,f),f'pair-{j}-{i}.stl')
report=dict(blender=bpy.app.version_string,units=source['units'],unit_scale=source['unit_scale'],original={o.name:stats(o) for o in objects})
# BVH overlaps are candidates, not an exact intersection certificate.
trees=[BVHTree.FromPolygons([v.co for v in o.data.vertices],[list(p.vertices) for p in o.data.polygons]) for o in objects]
report['cross_bvh_candidates']={f'{i}/{j}':len(trees[i].overlap(trees[j])) for i in range(len(objects)) for j in range(i+1,len(objects))}
verts=[];faces=[]
for o in source['objects']:
    offset=len(verts);verts.extend(o['vertices']);faces.extend([[i+offset for i in f] for f in o['faces']])
combined=make('23422-combined',verts,faces);export(combined,'23422-original.stl')
bm=bmesh.new();bm.from_mesh(combined.data)
bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=0.0)
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(combined.data);bm.free()
report['exact_coordinate_weld']=stats(combined)
export(combined,'23422-exact-weld.stl')
report['remesh_trials']={}
for mode in ('SHARP','VOXEL','VOXEL_015'):
    trial=make('remesh-'+mode,[list(v.co) for v in combined.data.vertices],[list(p.vertices) for p in combined.data.polygons])
    bpy.context.view_layer.objects.active=trial
    mod=trial.modifiers.new('Experimental remesh','REMESH');mod.mode='VOXEL' if mode.startswith('VOXEL') else mode
    if mode=='SHARP':
        mod.octree_depth=8;mod.use_remove_disconnected=False
    else:
        mod.voxel_size=0.15 if mode=='VOXEL_015' else 0.05;mod.adaptivity=0.0
    start=time.perf_counter()
    bpy.ops.object.modifier_apply(modifier=mod.name)
    report['remesh_trials'][mode]=stats(trial)
    report['remesh_trials'][mode]['seconds']=time.perf_counter()-start
    export(trial,'23422-remesh-'+mode.lower()+'.stl')
    if mode=='VOXEL_015':
        export(trial,'23422-blender-repaired.stl')
        candidate=trial
        # Independent sampled distances, without relaxing the service validator.
        from mathutils import Vector
        def surface_tree(obj):
            obj.data.calc_loop_triangles()
            return BVHTree.FromPolygons([v.co for v in obj.data.vertices],[list(t.vertices) for t in obj.data.loop_triangles],all_triangles=True)
        def deviation(a,b):
            tree=surface_tree(b);values=[];worst=[]
            a.data.calc_loop_triangles()
            for face in a.data.loop_triangles:
                p=[a.data.vertices[i].co for i in face.vertices]
                for u in range(5):
                    for v in range(5-u):
                        q=p[0]*(u/4)+p[1]*(v/4)+p[2]*(1-(u+v)/4)
                        loc,normal,index,distance=tree.find_nearest(q)
                        values.append(distance)
                        if not worst or distance>worst[0]:worst=[distance,list(q),list(loc)]
            values.sort()
            return dict(max=values[-1],p95=values[int(.95*(len(values)-1))],rms=(sum(d*d for d in values)/len(values))**.5,worst=worst,samples=len(values))
        report['sampled_fidelity']={'repair_to_source':deviation(candidate,combined),'source_to_repair':deviation(combined,candidate)}
(OUT/'analysis.json').write_text(json.dumps(report,indent=2))
print(json.dumps({k:v for k,v in report.items() if k!='original'}))
