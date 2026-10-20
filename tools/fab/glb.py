"""glTF 2.0 binary (GLB): read a model's triangles and what colors them, and
write the fabrication tool's one-mesh, one-texture result.

Reading is strict about what it takes in: a GLB whose triangles live in plain
or quantized accessors, with base colors from factors, vertex colors and
PNG or JPEG textures (embedded, or as data URIs). Anything that would change
the shape or its colors and that this reader does not decode (Draco or
meshopt compression, sparse accessors, a texture in a format other than PNG
or JPEG, an external file) is refused with the reason, never guessed at.

Every triangle comes out in the model's own frame with the scene's node
transforms applied, as a Part: positions, triangles, the UVs and colors its
material reads, and the material (base color factor, texture, sampler).
"""
import base64
import io
import json
import math
import struct

import numpy as np
from PIL import Image

GLB_MAGIC = 0x46546C67  # "glTF"
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

COMPONENT = {
    5120: (np.int8, 127.0),
    5121: (np.uint8, 255.0),
    5122: (np.int16, 32767.0),
    5123: (np.uint16, 65535.0),
    5125: (np.uint32, None),
    5126: (np.float32, None),
}
WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16}
# Extensions the reader honors, or that change nothing it reads.
UNDERSTOOD = {
    "KHR_mesh_quantization",       # integer accessors, decoded as such
    "KHR_texture_transform",       # applied to the base color's UVs
    "KHR_materials_emissive_strength", "KHR_materials_ior", "KHR_materials_specular",
    "KHR_materials_clearcoat", "KHR_materials_sheen", "KHR_materials_transmission",
    "KHR_materials_volume", "KHR_materials_iridescence", "KHR_materials_anisotropy",
    "KHR_materials_unlit", "KHR_materials_emissive", "KHR_materials_dispersion",
    "KHR_lights_punctual", "KHR_materials_variants", "KHR_xmp_json_ld",
}
WRAP_REPEAT, WRAP_CLAMP, WRAP_MIRROR = 10497, 33071, 33648


class GlbError(ValueError):
    """The input is not a model this reader takes; the message says why."""


class Material:
    def __init__(self, factor, image, wrap_s, wrap_t, uv_transform):
        self.factor = factor            # linear RGBA
        self.image = image              # PIL RGBA image (sRGB) or None
        self.wrap_s = wrap_s
        self.wrap_t = wrap_t
        self.uv_transform = uv_transform  # 3x3, applied to (u, v, 1)


class Part:
    def __init__(self, positions, triangles, uvs, colors, material):
        self.positions = positions      # (n, 3) float64, model frame
        self.triangles = triangles      # (m, 3) int64
        self.uvs = uvs                  # (n, 2) float64 or None
        self.colors = colors            # (n, 4) float64 linear or None
        self.material = material


def _chunks(data):
    if len(data) < 20:
        raise GlbError("too short to be a GLB")
    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC or version != 2:
        raise GlbError("not a glTF 2.0 binary (GLB)")
    if length != len(data):
        raise GlbError("the GLB's length field disagrees with the file (%d, %d)" % (length, len(data)))
    at, json_chunk, bin_chunk = 12, None, None
    while at + 8 <= len(data):
        size, kind = struct.unpack_from("<II", data, at)
        body = data[at + 8:at + 8 + size]
        if len(body) != size:
            raise GlbError("a chunk runs past the end of the file")
        if kind == CHUNK_JSON and json_chunk is None:
            json_chunk = body
        elif kind == CHUNK_BIN and bin_chunk is None:
            bin_chunk = body
        at += 8 + size
    if json_chunk is None:
        raise GlbError("no JSON chunk")
    return json.loads(json_chunk.decode("utf-8")), bin_chunk


def _data_uri(uri):
    if not uri.startswith("data:"):
        raise GlbError("refers to an external file (%r): a GLB must carry everything in itself" % uri[:60])
    head, _, payload = uri.partition(",")
    if not head.endswith(";base64"):
        raise GlbError("a data URI that is not base64")
    return base64.b64decode(payload)


class _Reader:
    def __init__(self, gltf, bin_chunk):
        self.gltf = gltf
        self.buffers = []
        for index, buffer in enumerate(gltf.get("buffers", [])):
            if "uri" in buffer:
                self.buffers.append(_data_uri(buffer["uri"]))
            elif index == 0 and bin_chunk is not None:
                self.buffers.append(bin_chunk)
            else:
                raise GlbError("buffer %d has no data" % index)
        self.images = {}

    def view_bytes(self, index):
        view = self.gltf["bufferViews"][index]
        data = self.buffers[view["buffer"]]
        start = view.get("byteOffset", 0)
        end = start + view["byteLength"]
        if end > len(data):
            raise GlbError("buffer view %d runs past its buffer" % index)
        return data[start:end], view.get("byteStride")

    def accessor(self, index):
        acc = self.gltf["accessors"][index]
        if "sparse" in acc:
            raise GlbError("sparse accessors are not taken")
        dtype, scale = COMPONENT[acc["componentType"]]
        width = WIDTH[acc["type"]]
        count = acc["count"]
        if "bufferView" not in acc:
            out = np.zeros((count, width), dtype=np.float64)
        else:
            raw, stride = self.view_bytes(acc["bufferView"])
            item = np.dtype(dtype).itemsize * width
            stride = stride or item
            start = acc.get("byteOffset", 0)
            if count and start + stride * (count - 1) + item > len(raw):
                raise GlbError("accessor %d runs past its buffer view" % index)
            rows = np.ndarray((count,), dtype=np.dtype((np.void, item)),
                              buffer=raw, offset=start, strides=(stride,))
            out = np.frombuffer(rows.tobytes(), dtype=dtype).reshape(count, width).astype(np.float64)
            if acc.get("normalized") and scale is not None:
                out = np.maximum(out / scale, -1.0)
        return out

    def image(self, index):
        if index in self.images:
            return self.images[index]
        image = self.gltf["images"][index]
        if "bufferView" in image:
            raw, _ = self.view_bytes(image["bufferView"])
        elif "uri" in image:
            raw = _data_uri(image["uri"])
        else:
            raise GlbError("image %d has no data" % index)
        try:
            decoded = Image.open(io.BytesIO(raw))
            if decoded.format not in ("PNG", "JPEG"):
                raise GlbError("image %d is %s, not PNG or JPEG" % (index, decoded.format))
            decoded = decoded.convert("RGBA")
        except GlbError:
            raise
        except Exception as error:  # Pillow's own refusal, with its reason
            raise GlbError("image %d cannot be decoded: %s" % (index, error)) from error
        self.images[index] = decoded
        return decoded


def _node_matrix(node):
    if "matrix" in node:
        return np.array(node["matrix"], dtype=np.float64).reshape(4, 4).T
    t = np.array(node.get("translation", [0.0, 0.0, 0.0]), dtype=np.float64)
    x, y, z, w = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    s = np.array(node.get("scale", [1.0, 1.0, 1.0]), dtype=np.float64)
    r = np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ])
    m = np.eye(4)
    m[:3, :3] = r * s[None, :]
    m[:3, 3] = t
    return m


def _uv_transform(info):
    ext = info.get("extensions", {}).get("KHR_texture_transform")
    if not ext:
        return np.eye(3), info.get("texCoord", 0)
    ox, oy = ext.get("offset", [0.0, 0.0])
    sx, sy = ext.get("scale", [1.0, 1.0])
    r = ext.get("rotation", 0.0)
    translation = np.array([[1, 0, ox], [0, 1, oy], [0, 0, 1]], dtype=np.float64)
    rotation = np.array([[math.cos(r), math.sin(r), 0], [-math.sin(r), math.cos(r), 0], [0, 0, 1]])
    scale = np.array([[sx, 0, 0], [0, sy, 0], [0, 0, 1]], dtype=np.float64)
    return translation @ rotation @ scale, ext.get("texCoord", info.get("texCoord", 0))


def _material(reader, index):
    if index is None:
        return Material(np.array([1.0, 1.0, 1.0, 1.0]), None, WRAP_REPEAT, WRAP_REPEAT, np.eye(3)), 0
    material = reader.gltf["materials"][index]
    pbr = material.get("pbrMetallicRoughness", {})
    factor = np.array(pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0]), dtype=np.float64)
    info = pbr.get("baseColorTexture")
    if info is None:
        return Material(factor, None, WRAP_REPEAT, WRAP_REPEAT, np.eye(3)), 0
    texture = reader.gltf["textures"][info["index"]]
    if "source" not in texture:
        raise GlbError("texture %d has no PNG or JPEG source" % info["index"])
    sampler = reader.gltf.get("samplers", [])[texture["sampler"]] if "sampler" in texture else {}
    transform, tex_coord = _uv_transform(info)
    return Material(factor, reader.image(texture["source"]), sampler.get("wrapS", WRAP_REPEAT),
                    sampler.get("wrapT", WRAP_REPEAT), transform), tex_coord


def read_parts(data):
    """Every triangle of the GLB's default scene, in the model's frame."""
    gltf, bin_chunk = _chunks(data)
    required = set(gltf.get("extensionsRequired", []))
    unknown = sorted(required - UNDERSTOOD)
    if unknown:
        raise GlbError("needs extensions this reader does not decode: %s" % ", ".join(unknown))
    reader = _Reader(gltf, bin_chunk)
    scenes = gltf.get("scenes", [])
    if scenes:
        roots = scenes[gltf.get("scene", 0)].get("nodes", [])
    else:
        roots = list(range(len(gltf.get("nodes", []))))
    parts = []
    stack = [(root, np.eye(4)) for root in reversed(roots)]
    seen = set()
    while stack:
        index, parent = stack.pop()
        if index in seen:
            raise GlbError("the node graph has a cycle")
        seen.add(index)
        node = gltf["nodes"][index]
        world = parent @ _node_matrix(node)
        for child in reversed(node.get("children", [])):
            stack.append((child, world))
        if "mesh" not in node:
            continue
        for primitive in gltf["meshes"][node["mesh"]]["primitives"]:
            mode = primitive.get("mode", 4)
            if mode not in (4, 5, 6):
                continue  # points and lines have no surface
            attrs = primitive["attributes"]
            if "POSITION" not in attrs:
                continue
            if "extensions" in primitive and set(primitive["extensions"]) & {
                    "KHR_draco_mesh_compression", "EXT_meshopt_compression"}:
                raise GlbError("compressed geometry (Draco or meshopt) is not taken")
            local = reader.accessor(attrs["POSITION"])[:, :3]
            positions = (np.c_[local, np.ones(len(local))] @ world.T)[:, :3]
            if "indices" in primitive:
                flat = reader.accessor(primitive["indices"])[:, 0].astype(np.int64)
            else:
                flat = np.arange(len(positions), dtype=np.int64)
            if mode == 4:
                triangles = flat[: len(flat) // 3 * 3].reshape(-1, 3)
            elif mode == 5:
                triangles = np.array([(flat[i], flat[i + 1], flat[i + 2]) if i % 2 == 0 else
                                      (flat[i + 1], flat[i], flat[i + 2]) for i in range(len(flat) - 2)],
                                     dtype=np.int64).reshape(-1, 3)
            else:
                triangles = np.array([(flat[0], flat[i], flat[i + 1]) for i in range(1, len(flat) - 1)],
                                     dtype=np.int64).reshape(-1, 3)
            if len(triangles) and (triangles.max() >= len(positions) or triangles.min() < 0):
                raise GlbError("an index points past its positions")
            if np.linalg.det(world[:3, :3]) < 0:
                triangles = triangles[:, ::-1]  # a mirrored node keeps its faces outward
            material, tex_coord = _material(reader, primitive.get("material"))
            uvs = None
            if material.image is not None:
                key = "TEXCOORD_%d" % tex_coord
                if key not in attrs:
                    raise GlbError("a textured primitive has no %s" % key)
                raw = reader.accessor(attrs[key])[:, :2]
                uvs = (np.c_[raw, np.ones(len(raw))] @ material.uv_transform.T)[:, :2]
            colors = None
            if "COLOR_0" in attrs:
                colors = reader.accessor(attrs["COLOR_0"])
                if colors.shape[1] == 3:
                    colors = np.c_[colors, np.ones(len(colors))]
            if not np.all(np.isfinite(positions)):
                raise GlbError("a position is not a finite number")
            parts.append(Part(positions, triangles, uvs, colors, material))
    if not parts or sum(len(p.triangles) for p in parts) == 0:
        raise GlbError("no triangles in the default scene")
    return parts


def _pad(blob, fill=b"\x00"):
    return blob + fill * ((4 - len(blob) % 4) % 4)


def write_glb(positions, normals, uvs, triangles, texture_bytes, texture_mime, material, name):
    """The tool's result: one node, one mesh, one primitive, one material with
    one base color texture. Positions in meters, the object's own frame."""
    positions = np.ascontiguousarray(positions, dtype=np.float32)
    normals = np.ascontiguousarray(normals, dtype=np.float32)
    uvs = np.ascontiguousarray(uvs, dtype=np.float32)
    index_type = np.uint16 if len(positions) < 65536 else np.uint32
    indices = np.ascontiguousarray(triangles.reshape(-1), dtype=index_type)
    blobs, views = [], []

    def add(blob, target=None):
        offset = sum(len(b) for b in blobs)
        blobs.append(_pad(blob))
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(blob)}
        if target is not None:
            view["target"] = target
        views.append(view)
        return len(views) - 1

    v_pos = add(positions.tobytes(), 34962)
    v_nrm = add(normals.tobytes(), 34962)
    v_uv = add(uvs.tobytes(), 34962)
    v_idx = add(indices.tobytes(), 34963)
    v_img = add(texture_bytes)
    accessors = [
        {"bufferView": v_pos, "componentType": 5126, "count": len(positions), "type": "VEC3",
         "min": [float(x) for x in positions.min(axis=0)], "max": [float(x) for x in positions.max(axis=0)]},
        {"bufferView": v_nrm, "componentType": 5126, "count": len(normals), "type": "VEC3"},
        {"bufferView": v_uv, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
        {"bufferView": v_idx, "componentType": 5123 if index_type is np.uint16 else 5125,
         "count": len(indices), "type": "SCALAR"},
    ]
    gltf = {
        "asset": {"version": "2.0", "generator": "ScraperX fabrication tool (tools/fab)"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}],
        "materials": [{"name": name, "pbrMetallicRoughness": {
            "baseColorTexture": {"index": 0}, "metallicFactor": material["metallic"],
            "roughnessFactor": material["roughness"]}}],
        "textures": [{"sampler": 0, "source": 0}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": WRAP_CLAMP, "wrapT": WRAP_CLAMP}],
        "images": [{"bufferView": v_img, "mimeType": texture_mime}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": sum(len(b) for b in blobs)}],
    }
    json_chunk = _pad(json.dumps(gltf, separators=(",", ":"), sort_keys=True).encode("utf-8"), b" ")
    bin_chunk = b"".join(blobs)
    total = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
    return (struct.pack("<III", GLB_MAGIC, 2, total) + struct.pack("<II", len(json_chunk), CHUNK_JSON) +
            json_chunk + struct.pack("<II", len(bin_chunk), CHUNK_BIN) + bin_chunk)
