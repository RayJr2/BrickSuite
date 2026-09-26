"""Synthetic positive and negative controls for the isolated rule."""
import unittest
import numpy as np
import trimesh
from proof import classify, lattice, nearest, TOL

def gate(source,repair,protected=None):
    if not repair.is_watertight or not repair.is_winding_consistent or repair.volume<=0:
        return False,'topology'
    p,ids=lattice(source);states,d=classify(repair,p)
    if np.any(np.isin(states,['outside','ambiguous'])):return False,'outside or ambiguous source'
    if protected is not None and np.any((d>TOL)&protected[ids]):return False,'protected surface buried'
    q,_=lattice(repair);_,distance,_=nearest(source,q)
    if distance.max()>TOL:return False,'new exterior'
    return True,'accepted'

class ProofControls(unittest.TestCase):
    def setUp(self):self.box=trimesh.creation.box(extents=[4,4,4])
    def test_unchanged(self):self.assertTrue(gate(self.box,self.box)[0])
    def test_internal_unprotected_sheet(self):
        sheet=trimesh.Trimesh(vertices=[[-1,-1,0],[1,-1,0],[0,1,0]],faces=[[0,1,2]],process=False)
        src=trimesh.util.concatenate([self.box,sheet])
        self.assertTrue(gate(src,self.box)[0])
        protected=np.zeros(len(src.faces),bool);protected[-1]=True
        self.assertEqual(gate(src,self.box,protected),(False,'protected surface buried'))
    def test_deleted_exterior_wall(self):
        bad=self.box.copy();bad.update_faces(np.arange(len(bad.faces))!=0)
        self.assertEqual(gate(self.box,bad),(False,'topology'))
    def test_bore_filled(self):
        source=trimesh.creation.annulus(r_min=1,r_max=2,height=4,sections=32)
        filled=trimesh.creation.cylinder(radius=2,height=4,sections=32)
        protected=np.linalg.norm(source.triangles_center[:,:2],axis=1)<1.1
        self.assertEqual(gate(source,filled,protected),(False,'protected surface buried'))
    def test_shortened_arm(self):
        bad=trimesh.creation.box(extents=[3,4,4])
        self.assertEqual(gate(self.box,bad),(False,'outside or ambiguous source'))
    def test_source_outside_repair(self):
        moved=self.box.copy();moved.apply_translation([.6,0,0])
        self.assertEqual(gate(moved,self.box),(False,'outside or ambiguous source'))
    def test_aabb_not_inside(self):
        ring=trimesh.creation.annulus(r_min=1,r_max=2,height=4,sections=32)
        states,_=classify(ring,np.array([[0.,0.,0.],[1.5,0,0],[3,0,0]]))
        self.assertEqual(states.tolist(),['outside','internal','outside'])
    def test_invalid_volume_cannot_prove_inside(self):
        states,_=classify(self.box,np.array([[0.,0.,0.]]),False)
        self.assertEqual(states.tolist(),['untrusted'])

if __name__=='__main__':unittest.main(verbosity=2)
