import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import numpy as np
import trimesh
from core import supports, classify_and_stitch, boundary
from worker import bounded


class ProofTests(unittest.TestCase):
    def setUp(self):
        base = Path(os.environ['PROOF_TEST_TMP']).resolve()
        self.temp = tempfile.TemporaryDirectory(dir=base)
        self.root = Path(self.temp.name).resolve()
        self.assertTrue(self.root.is_relative_to(base))

    def tearDown(self):
        self.temp.cleanup()

    def source_and_split(self):
        source = trimesh.Trimesh(vertices=[[0,0,0],[1,0,0],[0,1,0],[0,0,1]],
                                faces=[[0,2,1],[0,1,3],[1,2,3],[2,0,3]],process=False)
        split = trimesh.Trimesh(vertices=source.triangles.reshape(-1,3),
                               faces=np.arange(12).reshape(-1,3),process=False)
        ancestry = [{'triangle':i,'file':'synthetic.dat','line':i+1,'references':[{'reference':0}]} for i in range(4)]
        return source,split,ancestry

    def test_proven_authored_seams_close_without_patches_or_moving_geometry(self):
        source,mesh,ancestry = self.source_and_split()
        result,report = classify_and_stitch(mesh,source,ancestry,supports(mesh,source),[False]*4)
        self.assertEqual(report['applied_seams'],6)
        self.assertTrue(result.is_watertight)
        self.assertEqual(len(result.faces),4)
        self.assertEqual(report['generated_repair_triangles_added'],0)
        np.testing.assert_array_equal(result.triangles,mesh.triangles)

    def test_distance_alone_is_not_evidence(self):
        source,mesh,ancestry = self.source_and_split()
        result,report = classify_and_stitch(mesh,source,ancestry,[[] for _ in mesh.faces],[False]*4)
        self.assertEqual(report['applied_seams'],0)
        self.assertEqual(len(boundary(result)),12)
        self.assertTrue(all(p['classification']=='ambiguous/reject' for p in report['loops']))

    def test_protected_openings_never_stitch_or_cap(self):
        source,mesh,ancestry = self.source_and_split()
        result,report = classify_and_stitch(mesh,source,ancestry,supports(mesh,source),[False]*4,
                                           protected_edges=range(12))
        self.assertEqual(report['applied_seams'],0)
        self.assertTrue(all(p['classification']=='intentional opening' for p in report['loops']))
        self.assertTrue(all(not p['patch_allowed'] for p in report['loops']))

    def test_generated_face_cannot_claim_authored_seam(self):
        source,mesh,ancestry = self.source_and_split()
        _,report = classify_and_stitch(mesh,source,ancestry,supports(mesh,source),[True]*4)
        self.assertEqual(report['applied_seams'],0)

    def test_multiple_opposing_owners_are_ambiguous(self):
        source,mesh,ancestry = self.source_and_split()
        _,report = classify_and_stitch(mesh,source,ancestry,[[0,1]]*4,[False]*4)
        self.assertEqual(report['applied_seams'],0)

    def test_nonmanifold_authored_edge_cannot_be_used_as_seam_proof(self):
        source,mesh,ancestry = self.source_and_split()
        source = trimesh.Trimesh(vertices=source.vertices,faces=np.vstack([source.faces,source.faces]),process=False)
        _,report = classify_and_stitch(mesh,source,ancestry*2,[[0],[1],[2],[3]],[False]*4)
        self.assertEqual(report['applied_seams'],0)

    def test_provenance_replay_tags_only_new_patch(self):
        exe = os.environ['GEOGRAM_PROOF_EXE']
        source = self.root/'missing.off'
        source.write_text('OFF\n4 3 0\n0 0 0\n1 0 0\n0 1 0\n0 0 1\n3 0 2 1\n3 0 1 3\n3 1 2 3\n',encoding='utf-8')
        output = self.root/'job'
        state = bounded([exe,source,output/'mesh.off','fill-outer','0',output/'lineage.json'],output,'synthetic','tag patch')
        self.assertEqual(state['status'],'completed')
        lineage = json.loads((output/'lineage.json').read_text())['input_face_plus_one']
        self.assertEqual(sorted(lineage),[0,1,2,3])

    def run_child(self,code,**limits):
        import sys
        job = self.root/str(len(list(self.root.iterdir())))
        result = bounded([sys.executable,'-c',code],job,'synthetic-active-part','worker test',**limits)
        durable = json.loads((job/'worker-state.json').read_text())
        self.assertEqual(durable['part'],'synthetic-active-part')
        self.assertEqual(durable['exit_status'],result['exit_status'])
        self.assertTrue(Path(durable['diagnostic_path']).is_file())
        return result

    def test_worker_failure_does_not_stop_next_job(self):
        failure = self.run_child('raise SystemExit(7)')
        self.assertEqual(failure['exit_status'],7)
        self.assertEqual(failure['status'],'failed')
        self.assertEqual(self.run_child('print("next job survived")')['status'],'completed')

    def test_worker_wall_bound(self):
        result = self.run_child('import time; time.sleep(10)',wall_seconds=.1)
        self.assertTrue(result['timeout'])
        self.assertEqual(result['termination_reason'],'wall_timeout')

    def test_worker_cpu_bound(self):
        result = self.run_child('while True: pass',cpu_seconds=.1,wall_seconds=3)
        self.assertEqual(result['termination_reason'],'cpu_limit')

    def test_worker_memory_bound(self):
        result = self.run_child('import time; x=bytearray(64*1024**2); time.sleep(10)',memory_mib=32)
        self.assertEqual(result['termination_reason'],'memory_limit')

    def test_worker_output_bound(self):
        result = self.run_child('import sys,time; sys.stdout.write("x"*1048576); sys.stdout.flush(); time.sleep(10)',output_mib=.02)
        self.assertEqual(result['termination_reason'],'output_limit')


if __name__=='__main__':
    unittest.main()
