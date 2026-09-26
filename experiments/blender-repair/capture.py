"""Run in the loaded Blender scene before any experimental geometry edits."""
import bpy
import json
from pathlib import Path

OUT = Path('C:/Programming/Qt/BrickSuite/build/blender-repair-proof')
OUT.mkdir(parents=True, exist_ok=True)
backup = OUT / '23422-original-scene.blend'
if backup.exists():
    raise RuntimeError('Original scene backup already exists; do not overwrite it.')
bpy.ops.wm.save_as_mainfile(filepath=str(backup), copy=True)
objects = []
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH' or not obj.name.startswith('23422'):
        continue
    obj.data.calc_loop_triangles()
    objects.append(dict(name=obj.name, matrix=[list(row) for row in obj.matrix_world],
                        vertices=[list(obj.matrix_world @ v.co) for v in obj.data.vertices],
                        faces=[list(t.vertices) for t in obj.data.loop_triangles]))
(OUT / 'original-scene-meshes.json').write_text(json.dumps(dict(
    blender=bpy.app.version_string, units=bpy.context.scene.unit_settings.system,
    unit_scale=bpy.context.scene.unit_settings.scale_length, objects=objects)), encoding='utf-8')
print('Preserved scene and captured', len(objects), '23422 objects:', str(backup))
