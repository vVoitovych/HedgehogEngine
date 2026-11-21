"""Writes Projects/FeatureTest/Assets/Models/Animated/ThreeBoneArm.gltf.

A skinned arm of three joints one unit apart along +X, Shoulder at the origin, Elbow and Hand,
each carrying a box bound to it alone, and two clips:
  Wave  (2 s, to loop): the elbow swings 0 -> 45 -> 0 degrees about +Z.
  Reach (1 s, to loop): the shoulder slides 1 m along +X while the elbow unbends from 40 degrees,
        so an animator applying root motion moves its entity 1 m each loop.
Run from the repository root: python Tools/Animation/make_three_bone_arm.py
"""
import base64
import json
import math
import struct

OUT = "Projects/FeatureTest/Assets/Models/Animated/ThreeBoneArm.gltf"

positions, normals, uvs, joints, weights, indices = [], [], [], [], [], []


def box(joint, x0, x1, half):
    faces = [
        ((1, 0, 0), [(x1, -half, -half), (x1, half, -half), (x1, half, half), (x1, -half, half)]),
        ((-1, 0, 0), [(x0, -half, half), (x0, half, half), (x0, half, -half), (x0, -half, -half)]),
        ((0, 1, 0), [(x0, half, -half), (x0, half, half), (x1, half, half), (x1, half, -half)]),
        ((0, -1, 0), [(x0, -half, half), (x0, -half, -half), (x1, -half, -half), (x1, -half, half)]),
        ((0, 0, 1), [(x0, -half, half), (x1, -half, half), (x1, half, half), (x0, half, half)]),
        ((0, 0, -1), [(x0, half, -half), (x1, half, -half), (x1, -half, -half), (x0, -half, -half)]),
    ]
    for normal, corners in faces:
        base = len(positions)
        for i, corner in enumerate(corners):
            positions.append(corner)
            normals.append(normal)
            uvs.append([(0, 0), (1, 0), (1, 1), (0, 1)][i])
            joints.append((joint, 0, 0, 0))
            weights.append((1.0, 0.0, 0.0, 0.0))
        indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])


box(0, 0.0, 1.0, 0.12)
box(1, 1.0, 2.0, 0.1)
box(2, 2.0, 2.4, 0.08)


def about_z(degrees):
    half = math.radians(degrees) / 2
    return (0.0, 0.0, math.sin(half), math.cos(half))


blob = bytearray()
views, accessors = [], []


def add(data, fmt, count, kind, component, extra=None):
    while len(blob) % 4:
        blob.append(0)
    offset = len(blob)
    for item in data:
        blob.extend(struct.pack(fmt, *item) if isinstance(item, tuple) else struct.pack(fmt, item))
    views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset})
    accessor = {"bufferView": len(views) - 1, "componentType": component, "count": count, "type": kind}
    accessor.update(extra or {})
    accessors.append(accessor)
    return len(accessors) - 1


FLOAT, UBYTE, USHORT = 5126, 5121, 5123
xs = [p[0] for p in positions]
ys = [p[1] for p in positions]
zs = [p[2] for p in positions]
position = add(positions, "<3f", len(positions), "VEC3", FLOAT,
               {"min": [min(xs), min(ys), min(zs)], "max": [max(xs), max(ys), max(zs)]})
normal = add(normals, "<3f", len(normals), "VEC3", FLOAT)
uv = add(uvs, "<2f", len(uvs), "VEC2", FLOAT)
joint = add(joints, "<4B", len(joints), "VEC4", UBYTE)
weight = add(weights, "<4f", len(weights), "VEC4", FLOAT)
index = add(indices, "<H", len(indices), "SCALAR", USHORT)
inverse = add([m for x in (0.0, 1.0, 2.0)
               for m in (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x, 0, 0, 1)], "<f", 3, "MAT4", FLOAT)

wave_times = add([0.0, 1.0, 2.0], "<f", 3, "SCALAR", FLOAT, {"min": [0.0], "max": [2.0]})
wave_turns = add([about_z(0), about_z(45), about_z(0)], "<4f", 3, "VEC4", FLOAT)
reach_times = add([0.0, 1.0], "<f", 2, "SCALAR", FLOAT, {"min": [0.0], "max": [1.0]})
reach_slide = add([(0.0, 0.0, 0.0), (1.0, 0.0, 0.0)], "<3f", 2, "VEC3", FLOAT)
reach_turns = add([about_z(40), about_z(0)], "<4f", 2, "VEC4", FLOAT)


def sampler(times, values):
    return {"input": times, "output": values, "interpolation": "LINEAR"}


def channel(sampler_index, node, path):
    return {"sampler": sampler_index, "target": {"node": node, "path": path}}


gltf = {
    "asset": {"version": "2.0", "generator": "HedgehogEngine Tools/Animation/make_three_bone_arm.py"},
    "scene": 0,
    "scenes": [{"nodes": [0, 1]}],
    "nodes": [
        {"name": "Arm", "mesh": 0, "skin": 0},
        {"name": "Shoulder", "children": [2]},
        {"name": "Elbow", "translation": [1, 0, 0], "children": [3]},
        {"name": "Hand", "translation": [1, 0, 0]},
    ],
    "meshes": [{"name": "Arm", "primitives": [{"attributes": {
        "POSITION": position, "NORMAL": normal, "TEXCOORD_0": uv, "JOINTS_0": joint, "WEIGHTS_0": weight},
        "indices": index}]}],
    "skins": [{"joints": [1, 2, 3], "inverseBindMatrices": inverse}],
    "animations": [
        {"name": "Wave", "samplers": [sampler(wave_times, wave_turns)], "channels": [channel(0, 2, "rotation")]},
        {"name": "Reach", "samplers": [sampler(reach_times, reach_slide), sampler(reach_times, reach_turns)],
         "channels": [channel(0, 1, "translation"), channel(1, 2, "rotation")]},
    ],
    "buffers": [{"byteLength": len(blob),
                 "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode()}],
    "bufferViews": views,
    "accessors": accessors,
}

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    json.dump(gltf, f, indent=1)
    f.write("\n")
print("Wrote", OUT, len(blob), "bytes of buffer")
