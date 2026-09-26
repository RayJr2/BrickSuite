"""Synthetic fixtures only; no private benchmark is required for tests."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import numpy as np
import trimesh
from run import read_obj, save, align, topology


class ProofTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(dir=os.environ['PROOF_TEST_TMP'])
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def repaired(self, mesh, mode):
        source = self.root/'input.off'
        output = self.root/'output.off'
        save(mesh, source)
        before = source.read_bytes()
        result = subprocess.run([os.environ['GEOGRAM_PROOF_EXE'],str(source),str(output),mode,'.0004'],
                                capture_output=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stdout.decode(errors='replace'))
        self.assertEqual(source.read_bytes(),before)
        return trimesh.load_mesh(output,process=False)

    def test_obj_preserves_position_indices_across_normal_seams(self):
        p = self.root/'seams.obj'
        p.write_text('v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n'
                     'f 1/1/1 2/2/1 3/3/1\nf -4/7/2 -1/8/2 -3/9/2\n',encoding='utf-8')
        mesh = read_obj(p)
        self.assertEqual(len(mesh.vertices),4)
        np.testing.assert_array_equal(mesh.faces,[[0,1,2],[0,3,1]])

    def test_explicit_point_four_normalization_not_fitted_scale(self):
        source = trimesh.creation.box(extents=[2,4,6])
        studio = source.copy()
        studio.vertices = source.vertices*2.5+[10,20,30]
        normalized, registration = align(studio,source)
        self.assertEqual(registration['scale'],.4)
        self.assertFalse(registration['scale_fitted'])
        np.testing.assert_allclose(normalized.extents,source.extents,atol=1e-12)

    def test_hole_filling_is_explicit(self):
        mesh = trimesh.Trimesh(vertices=[[0,0,0],[1,0,0],[0,1,0],[0,0,1]],
                              faces=[[0,2,1],[0,1,3],[1,2,3]],process=False)
        self.assertGreater(topology(self.repaired(mesh,'clean'))['boundary_edges'],0)
        closed = self.repaired(mesh,'fill-outer')
        self.assertTrue(closed.is_watertight)
        self.assertEqual(len(closed.faces),4)

    def test_outer_extraction_preserves_through_bore(self):
        mesh = trimesh.creation.annulus(r_min=2,r_max=3,height=4,sections=16)
        repaired = self.repaired(mesh,'outer')
        self.assertTrue(repaired.is_watertight)
        self.assertEqual(repaired.euler_number,0)
        hits = repaired.ray.intersects_location([[0,0,-5]],[[0,0,1]])[0]
        self.assertEqual(len(hits),0)
        np.testing.assert_allclose(repaired.bounds,mesh.bounds,atol=1e-12)


if __name__ == '__main__':
    unittest.main()
