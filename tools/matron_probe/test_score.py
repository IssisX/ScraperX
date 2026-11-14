"""Pin the scorer's mesh geometry on known meshes.

tools/matron_probe/score.py decides the drawn-body verdicts (hair and skirt
penetration) from _tri_dist, _ray_hits and mesh_inside. Until now no test
covered any of them: a scorer bug read as a body verdict, and the M1
fresh-look investigation (issue #3, 2026-10-31) had to ask whether a game
fail was real or a scorer artefact with no unit test to point at. These
tests pin the contract on a unit cube and single triangles, where every
number is exact by construction. Run: (cd tools/matron_probe &&
python3 -m unittest -q test_score), or from the repo root as
python3 -m unittest tools.matron_probe.test_score; CI runs the first
command in the scope job on every push, next to the proof-scope unit tests.

Known limitation, measured 2026-11-11 and pinned, not fixed: mesh_inside
on the exact center of the cube returns False. All six axis rays from the
center hit a face center, which sits on the shared diagonal, so each
direction counts 2 (even) and no direction is odd. The two-direction rule
("one ray grazing a shared edge cannot fake it") assumes at most one
grazing direction; the symmetric center grazes all six. Real hair points
are never at a mesh symmetry center, and changing the parity rule would
move M1 verdicts, so the rule stays as documented until an owner ruling
says otherwise. test_cube_center_is_a_documented_singularity locks the
current behavior so a future parity change fails loudly here first.
"""

import unittest

import numpy as np

try:
    import score
except ModuleNotFoundError:  # run as tools.matron_probe.test_score from the repo root
    from tools.matron_probe import score


def cube_tris(half=0.5):
    """Twelve triangles of the cube [-half, half]^3, as (A, B, C) arrays."""
    v = {}
    for x in (-half, half):
        for y in (-half, half):
            for z in (-half, half):
                v[(x, y, z)] = np.array([x, y, z], float)
    faces = [
        [(half, -half, -half), (half, half, -half), (half, half, half), (half, -half, half)],
        [(-half, -half, -half), (-half, -half, half), (-half, half, half), (-half, half, -half)],
        [(-half, half, -half), (half, half, -half), (half, half, half), (-half, half, half)],
        [(-half, -half, -half), (-half, -half, half), (half, -half, half), (half, -half, -half)],
        [(-half, -half, half), (half, -half, half), (half, half, half), (-half, half, half)],
        [(-half, -half, -half), (-half, half, -half), (half, half, -half), (half, -half, -half)],
    ]
    a, b, c = [], [], []
    for f in faces:
        for tri in ((f[0], f[1], f[2]), (f[0], f[2], f[3])):
            a.append(v[tri[0]])
            b.append(v[tri[1]])
            c.append(v[tri[2]])
    return (np.array(a), np.array(b), np.array(c))


def shift_tris(tris, dx):
    a, b, c = tris
    d = np.array(dx, float)
    return (a + d, b + d, c + d)


def empty_tris():
    e = np.zeros((0, 3))
    return (e, e, e)


class TriDist(unittest.TestCase):
    def test_above_face_is_plane_distance(self):
        a = np.array([[0.0, 0.0, 0.0]])
        b = np.array([[1.0, 0.0, 0.0]])
        c = np.array([[0.0, 1.0, 0.0]])
        for z in (0.3, -0.3):
            d = score._tri_dist(np.array([0.25, 0.25, z]), a, b, c)
            self.assertAlmostEqual(float(d[0]), 0.3, places=9)

    def test_beyond_edge_is_edge_distance(self):
        a = np.array([[0.0, 0.0, 0.0]])
        b = np.array([[1.0, 0.0, 0.0]])
        c = np.array([[0.0, 1.0, 0.0]])
        d = score._tri_dist(np.array([2.0, 0.0, 0.0]), a, b, c)
        self.assertAlmostEqual(float(d[0]), 1.0, places=9)

    def test_on_vertex_is_zero(self):
        a = np.array([[0.0, 0.0, 0.0]])
        b = np.array([[1.0, 0.0, 0.0]])
        c = np.array([[0.0, 1.0, 0.0]])
        d = score._tri_dist(np.array([0.0, 0.0, 0.0]), a, b, c)
        self.assertAlmostEqual(float(d[0]), 0.0, places=9)

    def test_degenerate_triangle_is_segment_distance(self):
        a = np.array([[0.0, 0.0, 0.0]])
        b = np.array([[0.0, 0.0, 0.0]])
        c = np.array([[0.0, 0.0, 0.0]])
        d = score._tri_dist(np.array([3.0, 4.0, 0.0]), a, b, c)
        self.assertAlmostEqual(float(d[0]), 5.0, places=9)

    def test_cube_center_min_distance_is_half_side(self):
        a, b, c = cube_tris()
        d = score._tri_dist(np.array([0.0, 0.0, 0.0]), a, b, c)
        self.assertAlmostEqual(float(d.min()), 0.5, places=9)


class RayHits(unittest.TestCase):
    def setUp(self):
        self.a = np.array([[0.0, 0.0, 0.0]])
        self.b = np.array([[1.0, 0.0, 0.0]])
        self.c = np.array([[0.0, 1.0, 0.0]])

    def test_one_crossing_from_below(self):
        self.assertEqual(score._ray_hits(np.array([0.25, 0.25, -1.0]),
                                         np.array([0.0, 0.0, 1.0]), self.a, self.b, self.c), 1)

    def test_one_crossing_from_above(self):
        self.assertEqual(score._ray_hits(np.array([0.25, 0.25, 1.0]),
                                         np.array([0.0, 0.0, -1.0]), self.a, self.b, self.c), 1)

    def test_no_crossing_off_to_the_side(self):
        self.assertEqual(score._ray_hits(np.array([2.0, 2.0, -1.0]),
                                         np.array([0.0, 0.0, 1.0]), self.a, self.b, self.c), 0)

    def test_no_crossing_parallel_to_the_plane(self):
        self.assertEqual(score._ray_hits(np.array([0.25, 0.25, -1.0]),
                                         np.array([1.0, 0.0, 0.0]), self.a, self.b, self.c), 0)

    def test_cube_off_diagonal_ray_counts_once_each_way(self):
        a, b, c = cube_tris()
        p = np.array([0.0, 0.1, 0.05])
        self.assertEqual(score._ray_hits(p, np.array([1.0, 0.0, 0.0]), a, b, c), 1)
        p = np.array([2.0, 0.1, 0.05])
        self.assertEqual(score._ray_hits(p, np.array([-1.0, 0.0, 0.0]), a, b, c), 2)


class Aabb(unittest.TestCase):
    def test_inside_box_is_zero(self):
        lo = np.array([-0.5, -0.5, -0.5])
        hi = np.array([0.5, 0.5, 0.5])
        self.assertAlmostEqual(score._aabb_dist(np.array([0.0, 0.0, 0.0]), lo, hi), 0.0)

    def test_outside_box_is_axis_distance(self):
        lo = np.array([-0.5, -0.5, -0.5])
        hi = np.array([0.5, 0.5, 0.5])
        self.assertAlmostEqual(score._aabb_dist(np.array([1.5, 0.0, 0.0]), lo, hi), 1.0)
        self.assertAlmostEqual(score._aabb_dist(np.array([1.5, 1.5, 1.5]), lo, hi),
                               float(np.sqrt(3.0)))

    def test_ray_reaches_box(self):
        lo = np.array([-0.5, -0.5, -0.5])
        hi = np.array([0.5, 0.5, 0.5])
        self.assertTrue(score._ray_hits_aabb(np.array([2.0, 0.0, 0.0]),
                                             np.array([-1.0, 0.0, 0.0]), lo, hi))
        self.assertFalse(score._ray_hits_aabb(np.array([2.0, 0.0, 0.0]),
                                              np.array([1.0, 0.0, 0.0]), lo, hi))
        self.assertFalse(score._ray_hits_aabb(np.array([2.0, 2.0, 0.0]),
                                              np.array([-1.0, 0.0, 0.0]), lo, hi))
        self.assertTrue(score._ray_hits_aabb(np.array([0.0, 0.0, 0.0]),
                                             np.array([1.0, 0.0, 0.0]), lo, hi))
        self.assertFalse(score._ray_hits_aabb(np.array([0.5, 0.0, 0.0]),
                                              np.array([1.0, 0.0, 0.0]), lo, hi))


class MeshInside(unittest.TestCase):
    def setUp(self):
        self.cube = cube_tris()

    def test_off_center_interior_is_inside(self):
        inside, d = score.mesh_inside(np.array([0.1, 0.05, -0.07]), [self.cube])
        self.assertTrue(inside)
        self.assertAlmostEqual(d, 0.4, places=9)

    def test_interior_near_a_face(self):
        inside, d = score.mesh_inside(np.array([0.0, 0.0, 0.25]), [self.cube])
        self.assertTrue(inside)
        self.assertAlmostEqual(d, 0.25, places=9)

    def test_exterior_points_are_outside(self):
        for p, d in (([0.0, 0.0, 0.75], 0.25), ([2.0, 0.0, 0.0], 1.5)):
            inside, got = score.mesh_inside(np.array(p, float), [self.cube])
            self.assertFalse(inside)
            self.assertAlmostEqual(got, d, places=9)

    def test_open_mesh_needs_two_odd_directions(self):
        # One triangle, open mesh: the -z ray crosses once (odd) and no other
        # direction crosses, so one odd direction is not enough to be inside.
        a = np.array([[0.0, 0.0, 0.0]])
        b = np.array([[1.0, 0.0, 0.0]])
        c = np.array([[0.0, 1.0, 0.0]])
        inside, d = score.mesh_inside(np.array([0.25, 0.25, 0.001]), [(a, b, c)])
        self.assertFalse(inside)
        self.assertAlmostEqual(d, 0.001, places=9)

    def test_cube_center_is_a_documented_singularity(self):
        # See the module docstring: every axis ray from the exact center
        # grazes a face diagonal and counts 2, so no direction is odd.
        # This locks the current behavior; changing the parity rule needs
        # an owner ruling (M1 verdicts rest on it).
        inside, d = score.mesh_inside(np.array([0.0, 0.0, 0.0]), [self.cube])
        self.assertFalse(inside)
        self.assertAlmostEqual(d, 0.5, places=9)

    def test_precomputed_aabb_matches_computed(self):
        a, b, c = self.cube
        lo = np.minimum(np.minimum(a.min(axis=0), b.min(axis=0)), c.min(axis=0))
        hi = np.maximum(np.maximum(a.max(axis=0), b.max(axis=0)), c.max(axis=0))
        for p in ([0.1, 0.05, -0.07], [0.0, 0.0, 0.75], [2.0, 0.0, 0.0]):
            p = np.array(p, float)
            self.assertEqual(score.mesh_inside(p, [self.cube]),
                             score.mesh_inside(p, [self.cube], [(lo, hi)]))

    def test_far_part_and_empty_part_change_nothing(self):
        far = shift_tris(self.cube, (100.0, 0.0, 0.0))
        for p in ([0.1, 0.05, -0.07], [0.0, 0.0, 0.75]):
            p = np.array(p, float)
            base = score.mesh_inside(p, [self.cube])
            self.assertEqual(base, score.mesh_inside(p, [self.cube, far]))
            self.assertEqual(base, score.mesh_inside(p, [empty_tris(), self.cube]))


if __name__ == "__main__":
    unittest.main()
