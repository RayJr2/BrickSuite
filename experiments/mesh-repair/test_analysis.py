import unittest
import numpy as np
import trimesh
from analyze import directed, ownership, topology, features

class Measurements(unittest.TestCase):
    def test_parallel_planes(self):
        a=trimesh.Trimesh(vertices=[[0,0,0],[1,0,0],[0,1,0]],faces=[[0,1,2]],process=False)
        b=a.copy();b.vertices[:,2]+=2
        for x,y in [(a,b),(b,a)]:
            result=directed(x,y,100)
            self.assertAlmostEqual(result['sampled_max_mm'],2)
            self.assertAlmostEqual(result['area_sample_rms_mm'],2)
        self.assertEqual(topology(a)['boundary_edges'],3)

    def test_conservative_ownership(self):
        a=trimesh.Trimesh(vertices=[[0,0,0],[1,0,0],[0,1,0]],faces=[[0,1,2]],process=False)
        ancestry=[{'file':'source','line':1,'references':[]}]
        self.assertEqual(ownership(a,a,ancestry)['face_counts']['unique_triangle'],1)
        b=a.copy();b.vertices[:,2]+=.1
        self.assertEqual(ownership(b,a,ancestry)['face_counts']['new_or_unmatched'],1)

    def test_bore_probe_detects_closed_cap(self):
        cylinder=trimesh.creation.cylinder(radius=3,height=2)
        self.assertTrue(all(len(h)==2 for h in features(cylinder)['bore_axial_probe_hits']))

if __name__=='__main__':unittest.main()
