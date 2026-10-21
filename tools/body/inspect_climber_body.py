#!/usr/bin/env python3
"""Inspect the shipped climber GLB/rig without Godot or third-party modules."""
import hashlib
import json
from pathlib import Path
import struct
import sys


def inspect(directory):
    raw = (directory / "climber_body.glb.bytes").read_bytes()
    magic, version, size = struct.unpack_from("<4sII", raw)
    if (magic, version, size) != (b"glTF", 2, len(raw)):
        raise ValueError("climber GLB: invalid version/header/declared size")
    length, kind = struct.unpack_from("<II", raw, 12)
    if kind != 0x4E4F534A:
        raise ValueError("climber GLB: first chunk must be JSON")
    gltf = json.loads(raw[20:20 + length])
    rig = json.loads((directory / "climber_body_rig.json").read_text())
    nodes, accessors = gltf["nodes"], gltf["accessors"]
    joints = gltf["skins"][0]["joints"]
    if len(gltf["skins"]) != 1 or len(joints) != len(rig["bones"]):
        raise ValueError("climber GLB/rig: skin/bone count mismatch")
    parents = {child: i for i, node in enumerate(nodes)
               for child in node.get("children", [])}
    for joint, bone in zip(joints, rig["bones"]):
        node = nodes[joint]
        parent = parents.get(joint)
        parent_name = nodes[parent]["name"] if parent in joints else None
        if (node["name"], parent_name, node.get("translation"), node.get("rotation")) != (
                bone["name"], bone["parent"], bone["t"], bone["q"]):
            raise ValueError("climber GLB/rig: rest-pose mismatch for " + bone["name"])
    parts = []
    for mesh in gltf["meshes"]:
        vertices = triangles = 0
        for primitive in mesh["primitives"]:
            if primitive.get("mode", 4) != 4:
                raise ValueError("climber GLB: expected triangle list")
            attributes = primitive["attributes"]
            count = accessors[attributes["POSITION"]]["count"]
            if any(accessors[attributes[key]]["count"] != count
                   for key in ("JOINTS_0", "WEIGHTS_0")):
                raise ValueError("climber GLB: skin attribute count mismatch")
            if accessors[attributes["WEIGHTS_0"]]["type"] != "VEC4":
                raise ValueError("climber GLB: expected four skin weights per vertex")
            indices = accessors[primitive["indices"]]["count"]
            if indices % 3:
                raise ValueError("climber GLB: incomplete triangle")
            vertices += count
            triangles += indices // 3
        if triangles != rig["stats"]["triangles"][mesh["name"]]:
            raise ValueError("climber GLB/rig: triangle count mismatch for " + mesh["name"])
        parts.append({"name": mesh["name"], "surfaces": len(mesh["primitives"]),
                      "vertices": vertices, "triangles": triangles})
    if sum(p["triangles"] for p in parts) != rig["stats"]["triangles_total"]:
        raise ValueError("climber GLB/rig: total triangle count mismatch")
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest(),
            "bones": len(joints), "skins": len(gltf["skins"]),
            "materials": len(gltf["materials"]), "textures": len(gltf.get("textures", [])),
            "vertices": sum(p["vertices"] for p in parts),
            "triangles": sum(p["triangles"] for p in parts), "parts": parts,
            "glb_rig_rest_pose_and_counts_match": True}


if __name__ == "__main__":
    directory = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[2] / "godot/presentation/life/climber")
    print(json.dumps(inspect(directory), indent=2))
