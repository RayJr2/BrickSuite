import unittest,sys
from pathlib import Path
import numpy as np
import trimesh
from certificate import certify,convex_source_cells
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'asymmetric-fidelity'))
from test_proof import gate

class CertificateControls(unittest.TestCase):
    def setUp(self):self.box=trimesh.creation.box(extents=[4,4,4])
    def test_authored_closed_enclosure(self):
        cells=convex_source_cells(self.box)
        self.assertEqual(len(cells),1)
        self.assertEqual(certify(self.box,np.array([0.,0.,0.]),np.array([0.,0.,1.]),cells=cells)['classification'],'certified_internal')
    def test_exterior_wall(self):
        c=certify(self.box,np.array([0.,0.,2.]),np.array([0.,0.,1.]))
        self.assertEqual(c['classification'],'required_exterior')
        broken=self.box.copy();broken.update_faces(np.arange(len(broken.faces))!=0)
        self.assertFalse(gate(self.box,broken)[0])
    def test_exposed_passage_wall_disappears(self):
        ring=trimesh.creation.annulus(r_min=1,r_max=2,height=4,sections=32)
        c=certify(ring,np.array([1.,0.,0.]),np.array([-1.,0.,0.]),protected=True)
        self.assertEqual(c['classification'],'protected_exposed')
        filled=trimesh.creation.cylinder(radius=2,height=4,sections=32)
        protected=np.linalg.norm(ring.triangles_center[:,:2],axis=1)<1.1
        self.assertFalse(gate(ring,filled,protected)[0])
    def test_bore_filled(self):
        ring=trimesh.creation.annulus(r_min=1,r_max=2,height=4,sections=32)
        self.assertFalse(gate(ring,trimesh.creation.cylinder(radius=2,height=4,sections=32),np.ones(len(ring.faces),bool))[0])
    def test_arm_shortened(self):self.assertFalse(gate(self.box,trimesh.creation.box(extents=[3,4,4]))[0])
    def test_source_outside(self):
        src=self.box.copy();src.apply_translation([.6,0,0]);self.assertFalse(gate(src,self.box)[0])
    def test_buried_ambiguous_surface(self):
        # Repair enclosure cannot create a source certificate. Source has an open roof.
        roof=trimesh.Trimesh(vertices=[[-10,-10,1],[10,-10,1],[0,10,1]],faces=[[0,1,2]],process=False)
        cells=convex_source_cells(roof);self.assertEqual(cells,[])
        c=certify(roof,np.array([0.,0.,0.]),np.array([0.,0.,1.]),cells=cells)
        self.assertEqual(c['classification'],'ambiguous')
    def test_protected_never_internal(self):
        c=certify(self.box,np.array([0.,0.,0.]),np.array([0.,0.,1.]),True,convex_source_cells(self.box))
        self.assertNotEqual(c['classification'],'certified_internal')
    def test_no_hole_filling_in_source_cells(self):
        mesh=self.box.copy();mesh.update_faces(np.arange(len(mesh.faces))!=0)
        before=mesh.faces.copy();self.assertEqual(convex_source_cells(mesh),[])
        self.assertTrue(np.array_equal(mesh.faces,before))

if __name__=='__main__':unittest.main(verbosity=2)
