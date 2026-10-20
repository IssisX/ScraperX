"""Made fit for the game (03_EXECUTION/LIVE_FABRICATION/DESIGN.md, "How it works"
3): a model scaled into the dispatch carrier, cut to a phone's budget with one
texture, split into convex hulls for the physics, and weighed by its material.

    fit(parts, size_m, material, triangles, texture_px, seed) -> Fitted

The steps, each on the model in its own frame (meters, +Y up):

1. scale: centered on the middle of its box and scaled whole so its longest
   side is the requested size, never more than the carrier's bound (1.2 m);
2. cut: its vertices welded where they meet and its triangles collapsed by
   quadric error (fast-simplification) to the triangle budget;
3. one texture: the cut mesh unwrapped into one atlas (xatlas), and every
   texel of it colored from the original's surface nearest it (its texture,
   base color and vertex colors), so the result has exactly one texture of
   texture_px square however many the original had;
4. hulls: the cut mesh split by approximate convex decomposition (CoACD,
   seeded) into at most 8 hulls, each kept to at most 32 points (the points
   that span the most of it) and kept within the model's box;
5. mass: each hull's volume, summed, times the material's density.

Nothing here is random but the decomposition's search, which is seeded.
"""
import io
import math

import numpy as np
from PIL import Image
from scipy.spatial import ConvexHull, cKDTree

import hullfile

# The look each material is drawn with (the texture carries its color).
LOOKS = {
    "steel": {"metallic": 0.6, "roughness": 0.55},
    "timber": {"metallic": 0.0, "roughness": 0.85},
    "rubber": {"metallic": 0.0, "roughness": 0.92},
    "concrete": {"metallic": 0.0, "roughness": 0.95},
}
CREASE_DEG = 50.0       # an edge sharper than this is drawn hard
UNWRAP_STEP = 0.93      # each try at packing one atlas eases the texel density by this
DILATE_TEXELS = 12      # the atlas's colors run this far past each chart
SAMPLE_SPACING = 1.0 / 160.0   # of the object's size: the nearest-surface search's grid


class FitError(ValueError):
    """The model cannot be made fit; the message says why."""


class Fitted:
    pass


def _srgb_to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def _linear_to_srgb(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1.0 / 2.4) - 0.055)


# ---- 1. scale -----------------------------------------------------------------

def scale_parts(parts, size_m):
    lo = np.min([p.positions.min(axis=0) for p in parts], axis=0)
    hi = np.max([p.positions.max(axis=0) for p in parts], axis=0)
    span = hi - lo
    if not np.all(np.isfinite(span)) or span.max() <= 0.0:
        raise FitError("the model has no extent")
    target = min(float(size_m), hullfile.CARRIER_BOUND_M)
    scale = target / float(span.max())
    center = (lo + hi) * 0.5
    for part in parts:
        part.positions = (part.positions - center) * scale
    return scale, span, target


def merge_parts(parts):
    """All parts as one mesh, each vertex keeping its part's UV and color."""
    positions, triangles, owner, uvs, colors = [], [], [], [], []
    base = 0
    for index, part in enumerate(parts):
        n = len(part.positions)
        positions.append(part.positions)
        triangles.append(part.triangles + base)
        owner.append(np.full(len(part.triangles), index, dtype=np.int64))
        uvs.append(part.uvs if part.uvs is not None else np.zeros((n, 2)))
        colors.append(part.colors if part.colors is not None else np.ones((n, 4)))
        base += n
    return (np.concatenate(positions), np.concatenate(triangles), np.concatenate(owner),
            np.concatenate(uvs), np.concatenate(colors))


# ---- 2. cut -------------------------------------------------------------------

def weld(positions, triangles, tolerance):
    keys = np.round(positions / tolerance).astype(np.int64)
    _, first, inverse = np.unique(keys, axis=0, return_index=True, return_inverse=True)
    welded = positions[first]
    tris = inverse.reshape(-1)[triangles]
    keep = (tris[:, 0] != tris[:, 1]) & (tris[:, 1] != tris[:, 2]) & (tris[:, 0] != tris[:, 2])
    tris = tris[keep]
    area = np.linalg.norm(np.cross(welded[tris[:, 1]] - welded[tris[:, 0]],
                                   welded[tris[:, 2]] - welded[tris[:, 0]]), axis=1)
    tris = tris[area > tolerance * tolerance]
    used, remap = np.unique(tris.reshape(-1), return_inverse=True)
    return welded[used], remap.reshape(-1, 3)


def cut(positions, triangles, budget):
    if len(triangles) <= budget:
        return positions, triangles
    import fast_simplification
    points, faces = positions, triangles
    for _ in range(4):
        reduction = 1.0 - budget / float(len(faces))
        points, faces = fast_simplification.simplify(points.astype(np.float64), faces.astype(np.int64),
                                                     target_reduction=reduction, agg=7)
        if len(faces) <= budget:
            break
    if len(faces) > budget:
        raise FitError("cut to %d triangles, over the budget of %d" % (len(faces), budget))
    return np.asarray(points, dtype=np.float64), np.asarray(faces, dtype=np.int64)


# ---- 3. one texture -----------------------------------------------------------

def unwrap(positions, triangles, texture_px):
    """One atlas of texture_px square, its charts as dense as fits: the texel
    density first the one at which the surface would fill the whole atlas,
    then eased down a step at a time until every chart packs into it (packed
    by brute force, 2 texels between charts; the bake's dilation fills the
    gutters)."""
    import xatlas
    area = 0.5 * np.linalg.norm(np.cross(positions[triangles[:, 1]] - positions[triangles[:, 0]],
                                         positions[triangles[:, 2]] - positions[triangles[:, 0]]), axis=1).sum()
    density = math.sqrt(texture_px * texture_px / max(area, 1e-12))
    for _ in range(60):
        atlas = xatlas.Atlas()
        atlas.add_mesh(positions.astype(np.float32), triangles.astype(np.uint32))
        chart_options = xatlas.ChartOptions()
        pack_options = xatlas.PackOptions()
        pack_options.resolution = texture_px
        pack_options.texels_per_unit = float(density)
        pack_options.padding = 2
        pack_options.bilinear = True
        pack_options.bruteForce = True
        atlas.generate(chart_options, pack_options)
        if atlas.atlas_count == 1:
            break
        density *= UNWRAP_STEP
    else:
        raise FitError("the unwrap does not fit one atlas of %d px" % texture_px)
    vmapping, indices, uvs = atlas[0]
    return np.asarray(vmapping, dtype=np.int64), np.asarray(indices, dtype=np.int64).reshape(-1, 3), \
        np.asarray(uvs, dtype=np.float64)


def _closest_on_triangles(p, a, b, c):
    """The point of each triangle (a, b, c) nearest p, as barycentric weights
    (Ericson, Real-Time Collision Detection, 5.1.5), for arrays of each."""
    ab, ac = b - a, c - a
    ap, bp, cp = p - a, p - b, p - c
    d1, d2 = np.einsum("ij,ij->i", ab, ap), np.einsum("ij,ij->i", ac, ap)
    d3, d4 = np.einsum("ij,ij->i", ab, bp), np.einsum("ij,ij->i", ac, bp)
    d5, d6 = np.einsum("ij,ij->i", ab, cp), np.einsum("ij,ij->i", ac, cp)
    va, vb, vc = d3 * d6 - d5 * d4, d5 * d2 - d1 * d6, d1 * d4 - d3 * d2
    with np.errstate(divide="ignore", invalid="ignore"):
        denom = va + vb + vc
        v = np.where(denom != 0.0, vb / denom, 0.0)
        w = np.where(denom != 0.0, vc / denom, 0.0)
        bary = np.stack([1.0 - v - w, v, w], axis=1)
        # The regions, later ones winning (Ericson's order is A, B, AB, C, AC, BC).
        e = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        bc = (va <= 0) & ((d4 - d3) >= 0) & ((d5 - d6) >= 0)
        bary = np.where(bc[:, None], np.stack([np.zeros_like(e), 1.0 - e, e], axis=1), bary)
        e = d2 / (d2 - d6)
        acr = (vb <= 0) & (d2 >= 0) & (d6 <= 0)
        bary = np.where(acr[:, None], np.stack([1.0 - e, np.zeros_like(e), e], axis=1), bary)
        cr = (d6 >= 0) & (d5 <= d6)
        bary = np.where(cr[:, None], np.array([0.0, 0.0, 1.0]), bary)
        e = d1 / (d1 - d3)
        abr = (vc <= 0) & (d1 >= 0) & (d3 <= 0)
        bary = np.where(abr[:, None], np.stack([1.0 - e, e, np.zeros_like(e)], axis=1), bary)
        br = (d3 >= 0) & (d4 <= d3)
        bary = np.where(br[:, None], np.array([0.0, 1.0, 0.0]), bary)
        ar = (d1 <= 0) & (d2 <= 0)
        bary = np.where(ar[:, None], np.array([1.0, 0.0, 0.0]), bary)
    bary = np.nan_to_num(bary, nan=1.0 / 3.0)
    point = bary[:, :1] * a + bary[:, 1:2] * b + bary[:, 2:] * c
    return point, bary


def _sample_image(image_linear, uv, wrap_s, wrap_t):
    """Bilinear samples of a linear-light RGBA image at UVs (glTF: (0, 0) the
    image's top left), with the sampler's wrap."""
    h, w = image_linear.shape[:2]

    def wrap(i, size, mode):
        if mode == 33071:  # clamp to edge
            return np.clip(i, 0, size - 1)
        if mode == 33648:  # mirrored repeat
            period = np.mod(i, 2 * size)
            return np.where(period < size, period, 2 * size - 1 - period)
        return np.mod(i, size)

    x = uv[:, 0] * w - 0.5
    y = uv[:, 1] * h - 0.5
    x0, y0 = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    fx, fy = (x - x0)[:, None], (y - y0)[:, None]
    xa, xb = wrap(x0, w, wrap_s), wrap(x0 + 1, w, wrap_s)
    ya, yb = wrap(y0, h, wrap_t), wrap(y0 + 1, h, wrap_t)
    top = image_linear[ya, xa] * (1 - fx) + image_linear[ya, xb] * fx
    bottom = image_linear[yb, xa] * (1 - fx) + image_linear[yb, xb] * fx
    return top * (1 - fy) + bottom * fy


def bake(parts, hi_positions, hi_triangles, hi_owner, hi_uvs, hi_colors, lo_positions, lo_triangles, lo_uvs,
         texture_px, object_size):
    """The atlas: each texel the color of the original surface nearest the cut
    mesh's point there (facing the same way where there is a choice)."""
    # The original's triangles, degenerate ones dropped, and points on them for
    # the nearest-surface search: a grid on each fine enough for the spacing.
    a, b, c = (hi_positions[hi_triangles[:, k]] for k in range(3))
    normal = np.cross(b - a, c - a)
    area2 = np.linalg.norm(normal, axis=1)
    good = area2 > 1e-14
    a, b, c, normal, area2 = a[good], b[good], c[good], normal[good], area2[good]
    tri_ids = np.nonzero(good)[0]
    normal = normal / area2[:, None]
    spacing = SAMPLE_SPACING * object_size
    longest = np.maximum.reduce([np.linalg.norm(b - a, axis=1), np.linalg.norm(c - b, axis=1),
                                 np.linalg.norm(a - c, axis=1)])
    steps = np.clip(np.ceil(longest / spacing), 1, 24).astype(np.int64)
    sample_points, sample_tris = [], []
    for n in np.unique(steps):
        grid = [(i / n, j / n) for i in range(n + 1) for j in range(n + 1 - i)]
        grid = np.array(grid, dtype=np.float64)
        rows = np.nonzero(steps == n)[0]
        for gu, gv in grid:
            sample_points.append(a[rows] * (1 - gu - gv) + b[rows] * gu + c[rows] * gv)
            sample_tris.append(rows)
    sample_points = np.concatenate(sample_points)
    sample_tris = np.concatenate(sample_tris)
    tree = cKDTree(sample_points)

    # The cut mesh's surface at every texel its charts cover.
    px = texture_px
    covered = np.zeros((px, px), dtype=bool)
    texel_points, texel_normals, texel_index = [], [], []
    la, lb, lc = (lo_positions[lo_triangles[:, k]] for k in range(3))
    lnormal = np.cross(lb - la, lc - la)
    lnormal /= np.maximum(np.linalg.norm(lnormal, axis=1), 1e-30)[:, None]
    for t in range(len(lo_triangles)):
        uv = lo_uvs[lo_triangles[t]] * px
        x_lo, y_lo = np.floor(uv.min(axis=0) - 0.5).astype(int)
        x_hi, y_hi = np.ceil(uv.max(axis=0) + 0.5).astype(int)
        x_lo, y_lo = max(x_lo, 0), max(y_lo, 0)
        x_hi, y_hi = min(x_hi, px - 1), min(y_hi, px - 1)
        if x_hi < x_lo or y_hi < y_lo:
            continue
        xs, ys = np.meshgrid(np.arange(x_lo, x_hi + 1), np.arange(y_lo, y_hi + 1))
        q = np.stack([xs.reshape(-1) + 0.5, ys.reshape(-1) + 0.5], axis=1)
        e0, e1 = uv[1] - uv[0], uv[2] - uv[0]
        det = e0[0] * e1[1] - e0[1] * e1[0]
        if abs(det) < 1e-12:
            continue
        d = q - uv[0]
        bu = (d[:, 0] * e1[1] - d[:, 1] * e1[0]) / det
        bv = (e0[0] * d[:, 1] - e0[1] * d[:, 0]) / det
        # A texel whose center is within half a texel of the triangle counts:
        # the chart's own edge texels are colored, not left to the dilation.
        margin = 0.5 * math.sqrt(2.0) / max(np.linalg.norm(e0), np.linalg.norm(e1), 1e-9)
        inside = (bu >= -margin) & (bv >= -margin) & (bu + bv <= 1.0 + margin)
        if not inside.any():
            continue
        bu, bv = np.clip(bu[inside], 0.0, 1.0), np.clip(bv[inside], 0.0, 1.0)
        over = bu + bv > 1.0
        bu[over], bv[over] = bu[over] / (bu[over] + bv[over]), bv[over] / (bu[over] + bv[over])
        cells = q[inside] - 0.5
        texel_points.append(la[t] * (1 - bu - bv)[:, None] + lb[t] * bu[:, None] + lc[t] * bv[:, None])
        texel_normals.append(np.repeat(lnormal[t][None, :], len(bu), axis=0))
        texel_index.append(cells[:, 1].astype(np.int64) * px + cells[:, 0].astype(np.int64))
    points = np.concatenate(texel_points)
    normals = np.concatenate(texel_normals)
    index = np.concatenate(texel_index)
    # A texel two triangles share keeps the last one's color; mark coverage.
    covered.reshape(-1)[index] = True

    images = [None if p.material.image is None else
              _srgb_to_linear(np.asarray(p.material.image, dtype=np.float64) / 255.0) for p in parts]
    color = np.zeros((px * px, 3), dtype=np.float64)
    k = 12
    for start in range(0, len(points), 20000):
        p = points[start:start + 20000]
        n = normals[start:start + 20000]
        _, near = tree.query(p, k=k)
        candidates = sample_tris[near]                       # (m, k)
        flat = candidates.reshape(-1)
        pp = np.repeat(p, k, axis=0)
        closest, bary = _closest_on_triangles(pp, a[flat], b[flat], c[flat])
        dist = np.linalg.norm(closest - pp, axis=1).reshape(-1, k)
        facing = np.einsum("ij,ij->i", np.repeat(n, k, axis=0), normal[flat]).reshape(-1, k)
        score = dist + np.where(facing < 0.0, 0.05 * object_size, 0.0)
        pick = np.argmin(score, axis=1)
        rows = np.arange(len(p))
        chosen = candidates[rows, pick]
        chosen_bary = bary.reshape(-1, k, 3)[rows, pick]
        original = tri_ids[chosen]
        corners = hi_triangles[original]
        uv = np.einsum("ij,ijk->ik", chosen_bary, hi_uvs[corners])
        vcolor = np.einsum("ij,ijk->ik", chosen_bary, hi_colors[corners])
        owner = hi_owner[original]
        out = np.ones((len(p), 4), dtype=np.float64)
        for o in np.unique(owner):
            mask = owner == o
            material = parts[o].material
            texel = np.ones((mask.sum(), 4)) if images[o] is None else \
                _sample_image(images[o], uv[mask], material.wrap_s, material.wrap_t)
            out[mask] = texel * material.factor[None, :] * vcolor[mask]
        color[index[start:start + 20000]] = out[:, :3]

    # Gutters: each empty texel next to colored ones takes their average, a
    # ring at a time; what is left takes the average color.
    color = color.reshape(px, px, 3)
    filled = covered.copy()
    mean = color[covered].mean(axis=0) if covered.any() else np.full(3, 0.5)
    for _ in range(DILATE_TEXELS):
        acc = np.zeros_like(color)
        cnt = np.zeros((px, px))
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx == 0 and dy == 0:
                    continue
                shifted = np.roll(np.roll(filled, dy, axis=0), dx, axis=1)
                acc += np.roll(np.roll(color * filled[..., None], dy, axis=0), dx, axis=1)
                cnt += shifted
        grow = (~filled) & (cnt > 0)
        color[grow] = acc[grow] / cnt[grow][:, None]
        filled |= grow
    color[~filled] = mean
    srgb = np.round(_linear_to_srgb(color) * 255.0).astype(np.uint8)
    return srgb, float(covered.mean())


def crease_normals(positions, triangles, uvs):
    """Vertex normals, a vertex split where faces meet sharper than CREASE_DEG
    (and where the unwrap already split it)."""
    a, b, c = (positions[triangles[:, k]] for k in range(3))
    face = np.cross(b - a, c - a)
    face /= np.maximum(np.linalg.norm(face, axis=1), 1e-30)[:, None]

    def angle(p, q, r):
        u, v = q - p, r - p
        cos = np.einsum("ij,ij->i", u, v) / np.maximum(np.linalg.norm(u, axis=1) * np.linalg.norm(v, axis=1), 1e-30)
        return np.arccos(np.clip(cos, -1.0, 1.0))

    weights = np.stack([angle(a, b, c), angle(b, c, a), angle(c, a, b)], axis=1)
    faces_of = [[] for _ in range(len(positions))]
    for f, tri in enumerate(triangles):
        for corner, v in enumerate(tri):
            faces_of[v].append((f, corner))
    limit = math.cos(math.radians(CREASE_DEG))
    out_positions, out_normals, out_uvs, out_triangles = [], [], [], np.zeros_like(triangles)
    for v, around in enumerate(faces_of):
        groups = []
        for f, corner in around:
            n = np.zeros(3)
            for g, other in around:
                if np.dot(face[f], face[g]) >= limit:
                    n += face[g] * weights[g, other]
            n /= max(np.linalg.norm(n), 1e-30)
            for gi, (gn, _) in enumerate(groups):
                if np.dot(gn, n) > 0.9999:
                    out_triangles[f, corner] = groups[gi][1]
                    break
            else:
                out_triangles[f, corner] = len(out_positions)
                groups.append((n, len(out_positions)))
                out_positions.append(positions[v])
                out_normals.append(n)
                out_uvs.append(uvs[v])
    return np.array(out_positions), np.array(out_normals), np.array(out_uvs), out_triangles


# ---- 4. hulls -----------------------------------------------------------------

def _spanning_points(points, most):
    """At most `most` of a hull's points: grown from its widest tetrahedron by
    the point farthest outside, so the hull kept is the one spanning most."""
    points = np.unique(np.round(points, 9), axis=0)
    if len(points) <= most:
        return points
    first = int(np.argmin(points[:, 0]))
    second = int(np.argmax(np.linalg.norm(points - points[first], axis=1)))
    line = points[second] - points[first]
    off = points - points[first]
    third = int(np.argmax(np.linalg.norm(np.cross(off, line), axis=1)))
    plane = np.cross(line, points[third] - points[first])
    fourth = int(np.argmax(np.abs(off @ plane)))
    chosen = [first, second, third, fourth]
    while len(chosen) < most:
        hull = ConvexHull(points[chosen])
        outside = (points @ hull.equations[:, :3].T + hull.equations[:, 3]).max(axis=1)
        outside[chosen] = -np.inf
        best = int(np.argmax(outside))
        if outside[best] <= 1e-6:
            break
        chosen.append(best)
    return points[sorted(chosen)]


def decompose(positions, triangles, half_box, seed):
    import coacd
    coacd.set_log_level("error")
    mesh = coacd.Mesh(positions.astype(np.float64), triangles.astype(np.int64))
    parts = coacd.run_coacd(mesh, threshold=0.05, max_convex_hull=hullfile.MAX_HULLS,
                            decimate=True, max_ch_vertex=hullfile.MAX_POINTS, seed=seed)
    hulls = []
    for vertices, _ in parts:
        pts = np.clip(np.asarray(vertices, dtype=np.float64), -half_box, half_box)
        pts = _spanning_points(pts, hullfile.MAX_POINTS)
        if len(pts) < hullfile.MIN_POINTS:
            continue
        # What the file holds is f32: the volume is the f32 points' own.
        pts = pts.astype(np.float32).astype(np.float64)
        try:
            hull = ConvexHull(pts)
        except Exception:
            continue
        if hull.volume < 1e-7:
            continue
        hulls.append((hull.volume, pts[hull.vertices]))
    if not hulls:
        raise FitError("the decomposition left no hull with a volume")
    if len(hulls) > hullfile.MAX_HULLS:
        raise FitError("the decomposition gave %d hulls, over %d" % (len(hulls), hullfile.MAX_HULLS))
    hulls.sort(key=lambda item: (-round(item[0], 12), tuple(np.round(item[1].mean(axis=0), 6))))
    return [h for _, h in hulls], [v for v, _ in hulls]


# ---- the whole --------------------------------------------------------------

def fit(parts, size_m, material, budget=4000, texture_px=512, seed=0):
    if material not in hullfile.MATERIALS:
        raise FitError("material %r: one of %s" % (material, ", ".join(hullfile.MATERIALS)))
    if not size_m > 0.0:
        raise FitError("the size must be more than nothing")
    scale, source_span, target = scale_parts(parts, size_m)
    hi_positions, hi_triangles, hi_owner, hi_uvs, hi_colors = merge_parts(parts)
    welded, welded_tris = weld(hi_positions, hi_triangles, 1e-6 * target)
    lo_positions, lo_triangles = cut(welded, welded_tris, budget)
    # A collapse's best point can sit a hair outside the original's box: kept
    # in it, so the cut mesh is never bigger than what was asked for.
    lo_positions = np.clip(lo_positions, hi_positions.min(axis=0), hi_positions.max(axis=0))
    vmapping, uv_triangles, uvs = unwrap(lo_positions, lo_triangles, texture_px)
    lo_vertices = lo_positions[vmapping]
    pixels, coverage = bake(parts, hi_positions, hi_triangles, hi_owner, hi_uvs, hi_colors, lo_vertices,
                            uv_triangles, uvs, texture_px, target)
    draw_positions, draw_normals, draw_uvs, draw_triangles = crease_normals(lo_vertices, uv_triangles, uvs)
    model_lo = draw_positions.min(axis=0)
    model_hi = draw_positions.max(axis=0)
    half_box = np.maximum(np.abs(model_lo), np.abs(model_hi))
    hulls, volumes = decompose(lo_positions, lo_triangles, half_box, seed)
    volume = float(sum(volumes))
    density = hullfile.MATERIALS[material][1]
    mass = density * volume
    extent = 2.0 * half_box
    if extent.max() > hullfile.CARRIER_BOUND_M + 1e-6:
        raise FitError("%.3f m: over the carrier's bound of %.1f m" % (extent.max(), hullfile.CARRIER_BOUND_M))
    if extent.min() < hullfile.MIN_EXTENT_M:
        raise FitError("%.3f m thin: under %.2f m on one side" % (extent.min(), hullfile.MIN_EXTENT_M))
    if mass > hullfile.MAX_MASS_KG:
        raise FitError("%.1f kg of %s at %.2f m: more than the hands carry (%.0f kg); order it smaller "
                       "or of a lighter material" % (mass, material, target, hullfile.MAX_MASS_KG))
    if mass < hullfile.MIN_MASS_KG:
        raise FitError("%.3f kg: under the least an object may weigh (%.1f kg)" % (mass, hullfile.MIN_MASS_KG))
    texture = io.BytesIO()
    Image.fromarray(pixels, "RGB").save(texture, format="JPEG", quality=90, optimize=True)
    out = Fitted()
    out.scale, out.source_span, out.size_m = scale, source_span, target
    out.positions, out.normals, out.uvs, out.triangles = draw_positions, draw_normals, draw_uvs, draw_triangles
    out.texture = texture.getvalue()
    out.texture_px = texture_px
    out.texture_coverage = coverage
    out.look = LOOKS[material]
    out.half_extent = tuple(float(x) for x in half_box.astype(np.float32))
    out.hulls, out.hull_volumes = hulls, volumes
    out.volume_m3, out.mass_kg, out.density = volume, mass, density
    out.source_triangles = len(hi_triangles)
    return out
