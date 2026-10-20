"""The hull file: what the native simulation builds a fabricated object's body
from (src/sim/fabricated.hpp reads it; the two must agree byte for byte).

Little-endian, no padding:

    offset  size  field
         0     8  magic "SXFABHUL"
         8     4  u32 version (1)
        12     4  u32 material: 0 steel, 1 timber, 2 rubber, 3 concrete
        16     4  u32 hull count, 1 to 8
        20     4  f32 mass, kg: the material's density times the hulls' volume
        24     4  f32 the hulls' volume, m^3 (each hull's own, summed)
        28    12  f32[3] half extents, m: the object's box about its origin,
                  which every hull point and the model lie within
        40    64  SHA-512 of the fitted GLB (the model drawn for it)
       104     .  each hull: u32 point count (4 to 32), then that many
                  f32[3] points, m, in the object's frame

The frame is the GLB's: meters, +Y up, the origin at the middle of the
object's box. The file is signed whole (fab.py sign_message), and the GLB is
bound to it by its hash, so one signature covers both.
"""
import struct

MAGIC = b"SXFABHUL"
VERSION = 1
HEADER = struct.Struct("<8sIIIff3f64s")
assert HEADER.size == 104
MAX_HULLS = 8
MIN_POINTS = 4
MAX_POINTS = 32
MAX_BYTES = HEADER.size + MAX_HULLS * (4 + MAX_POINTS * 12)
# Each a density (kg/m^3), as the native's table (src/sim/fabricated.cpp).
MATERIALS = {
    "steel": (0, 7850.0),
    "timber": (1, 600.0),
    "rubber": (2, 1100.0),
    "concrete": (3, 2400.0),
}
# The dispatch carrier's bound, each axis, m, and the least an object may be.
CARRIER_BOUND_M = 1.2
MIN_EXTENT_M = 0.02
# What the hands carry (the native's carry grip holds about 70 kg).
MIN_MASS_KG = 0.2
MAX_MASS_KG = 70.0


def pack(material, mass_kg, volume_m3, half_extent, glb_sha512, hulls):
    if not 1 <= len(hulls) <= MAX_HULLS:
        raise ValueError("%d hulls: a hull file holds 1 to %d" % (len(hulls), MAX_HULLS))
    out = bytearray(HEADER.pack(MAGIC, VERSION, MATERIALS[material][0], len(hulls), mass_kg, volume_m3,
                                *half_extent, glb_sha512))
    for hull in hulls:
        if not MIN_POINTS <= len(hull) <= MAX_POINTS:
            raise ValueError("a hull of %d points: each holds %d to %d" % (len(hull), MIN_POINTS, MAX_POINTS))
        out += struct.pack("<I", len(hull))
        for point in hull:
            out += struct.pack("<3f", *point)
    assert len(out) <= MAX_BYTES
    return bytes(out)


def unpack(data):
    """The file's fields, bounds-checked the way the native checks them."""
    if not HEADER.size <= len(data) <= MAX_BYTES:
        raise ValueError("%d bytes: a hull file is %d to %d" % (len(data), HEADER.size, MAX_BYTES))
    magic, version, material, count, mass, volume, hx, hy, hz, digest = HEADER.unpack_from(data, 0)
    if magic != MAGIC or version != VERSION:
        raise ValueError("not a version %d hull file" % VERSION)
    names = {code: name for name, (code, _) in MATERIALS.items()}
    if material not in names or not 1 <= count <= MAX_HULLS:
        raise ValueError("unknown material or hull count")
    at, hulls = HEADER.size, []
    for _ in range(count):
        (points,) = struct.unpack_from("<I", data, at)
        at += 4
        if not MIN_POINTS <= points <= MAX_POINTS or at + 12 * points > len(data):
            raise ValueError("a hull's point count is out of bounds")
        hulls.append([struct.unpack_from("<3f", data, at + 12 * i) for i in range(points)])
        at += 12 * points
    if at != len(data):
        raise ValueError("bytes after the last hull")
    return {"material": names[material], "mass_kg": mass, "volume_m3": volume, "half_extent": (hx, hy, hz),
            "glb_sha512": digest, "hulls": hulls}
