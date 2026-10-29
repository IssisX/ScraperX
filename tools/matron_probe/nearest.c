/* The nearest of a set of points to each query, by a uniform grid: score.py compiles this once
 * (cc -O2 -shared -fPIC) and calls it through ctypes; without a C compiler it runs a slower numpy
 * search instead, with the same results.
 *
 * pts, nrm: the points and their outward normals (npts x 3). keys, start, count: the grid's
 * occupied cells, sorted by key, and where each cell's points start in `order` and how many there
 * are. ring: cells searched round the query's own on each side. For each query: the nearest point
 * found (-1: none), its distance, and the signed distance (the distance to the tangent plane at the
 * nearest point when the query is behind it, never more than to the point itself; else the
 * distance).
 */
#include <math.h>
#include <stdint.h>

static int64_t find_cell(const uint64_t *keys, int64_t n, uint64_t k) {
    int64_t lo = 0, hi = n - 1;
    while (lo <= hi) {
        int64_t mid = (lo + hi) >> 1;
        uint64_t v = keys[mid];
        if (v == k) return mid;
        if (v < k) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

static uint64_t key3(int64_t x, int64_t y, int64_t z) {
    const int64_t off = (int64_t)1 << 20;
    return ((uint64_t)(x + off) << 42) | ((uint64_t)(y + off) << 21) | (uint64_t)(z + off);
}

void nearest(const double *pts, const double *nrm, const uint64_t *keys, const int64_t *start,
             const int64_t *count, int64_t ncells, const int64_t *order, double h, int64_t ring,
             const double *q, int64_t nq, int64_t *out_i, double *out_d, double *out_s) {
    for (int64_t i = 0; i < nq; i++) {
        const double qx = q[3 * i], qy = q[3 * i + 1], qz = q[3 * i + 2];
        const int64_t cx = (int64_t)floor(qx / h), cy = (int64_t)floor(qy / h), cz = (int64_t)floor(qz / h);
        double best = INFINITY;
        int64_t bi = -1;
        for (int64_t dx = -ring; dx <= ring; dx++)
            for (int64_t dy = -ring; dy <= ring; dy++)
                for (int64_t dz = -ring; dz <= ring; dz++) {
                    const int64_t c = find_cell(keys, ncells, key3(cx + dx, cy + dy, cz + dz));
                    if (c < 0) continue;
                    const int64_t end = start[c] + count[c];
                    for (int64_t k = start[c]; k < end; k++) {
                        const int64_t v = order[k];
                        const double ex = qx - pts[3 * v], ey = qy - pts[3 * v + 1], ez = qz - pts[3 * v + 2];
                        const double d2 = ex * ex + ey * ey + ez * ez;
                        if (d2 < best) { best = d2; bi = v; }
                    }
                }
        out_i[i] = bi;
        if (bi < 0) {
            out_d[i] = INFINITY;
            out_s[i] = INFINITY;
            continue;
        }
        const double d = sqrt(best);
        const double along = (qx - pts[3 * bi]) * nrm[3 * bi] + (qy - pts[3 * bi + 1]) * nrm[3 * bi + 1] +
                             (qz - pts[3 * bi + 2]) * nrm[3 * bi + 2];
        out_d[i] = d;
        out_s[i] = along < 0.0 ? (along > -d ? along : -d) : d;
    }
}
