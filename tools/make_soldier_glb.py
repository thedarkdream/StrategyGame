#!/usr/bin/env python3
"""Generates the placeholder soldier model assets/models/soldier.glb (stdlib only).

A box-built, skinned soldier, ~1.8 m tall, feet at y = 0, facing +Z (glTF convention),
with a 12-joint skeleton and three looping clips: idle, walk, attack.
Materials:  team_color  (tinted with the owner's team colour by the game),
            fatigues    (textured with an embedded camouflage PNG),
            skin, gear  (plain colours).
Replace the output with a real model any time; the game only needs the path and
the clip names (idle / walk / attack / gather).
"""
import json
import math
import os
import random
import struct
import sys
import zlib

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "models", "soldier.glb")

# (name, parent, absolute rest position).  Every joint starts unrotated.
JOINTS = [
    ("hips",       None,         (0.00, 0.90, 0.00)),
    ("spine",      "hips",       (0.00, 1.00, 0.00)),
    ("head",       "spine",      (0.00, 1.46, 0.00)),
    ("shoulder_l", "spine",      (-0.26, 1.34, 0.00)),
    ("elbow_l",    "shoulder_l", (-0.29, 1.10, 0.00)),
    ("shoulder_r", "spine",      (0.26, 1.34, 0.00)),
    ("elbow_r",    "shoulder_r", (0.29, 1.10, 0.00)),
    ("weapon",     "elbow_r",    (0.29, 0.90, 0.02)),
    ("hip_l",      "hips",       (-0.11, 0.88, 0.00)),
    ("knee_l",     "hip_l",      (-0.11, 0.46, 0.00)),
    ("hip_r",      "hips",       (0.11, 0.88, 0.00)),
    ("knee_r",     "hip_r",      (0.11, 0.46, 0.00)),
]
JOINT_INDEX = {name: i for i, (name, _, _) in enumerate(JOINTS)}

# (material, joint, centre xyz, size xyz) — each box moves rigidly with its joint.
BOXES = [
    # legs and boots
    ("fatigues", "hip_l",  (-0.11, 0.67, 0.00), (0.16, 0.42, 0.19)),
    ("fatigues", "knee_l", (-0.11, 0.28, 0.00), (0.14, 0.36, 0.17)),
    ("gear",     "knee_l", (-0.11, 0.05, 0.04), (0.17, 0.10, 0.28)),
    ("fatigues", "hip_r",  (0.11, 0.67, 0.00), (0.16, 0.42, 0.19)),
    ("fatigues", "knee_r", (0.11, 0.28, 0.00), (0.14, 0.36, 0.17)),
    ("gear",     "knee_r", (0.11, 0.05, 0.04), (0.17, 0.10, 0.28)),
    # pelvis, torso, vest, backpack
    ("fatigues", "hips",   (0.00, 0.92, 0.00), (0.40, 0.16, 0.24)),
    ("fatigues", "spine",  (0.00, 1.17, 0.00), (0.42, 0.34, 0.24)),
    ("team_color", "spine", (0.00, 1.16, 0.00), (0.45, 0.32, 0.27)),
    ("gear",     "spine",  (0.00, 1.16, -0.20), (0.30, 0.34, 0.14)),
    # arms
    ("fatigues", "shoulder_l", (-0.30, 1.22, 0.00), (0.12, 0.26, 0.14)),
    ("fatigues", "elbow_l",    (-0.30, 0.98, 0.00), (0.11, 0.24, 0.12)),
    ("skin",     "elbow_l",    (-0.30, 0.85, 0.00), (0.09, 0.09, 0.09)),
    ("fatigues", "shoulder_r", (0.30, 1.22, 0.00), (0.12, 0.26, 0.14)),
    ("fatigues", "elbow_r",    (0.30, 0.98, 0.00), (0.11, 0.24, 0.12)),
    ("skin",     "elbow_r",    (0.30, 0.85, 0.00), (0.09, 0.09, 0.09)),
    # head and helmet
    ("skin",     "head",   (0.00, 1.58, 0.01), (0.20, 0.22, 0.22)),
    ("team_color", "head", (0.00, 1.68, 0.00), (0.26, 0.13, 0.28)),
    # rifle, held in the right hand
    ("gear",     "weapon", (0.30, 0.92, 0.26), (0.07, 0.10, 0.50)),
]

MATERIALS = {
    "team_color": {"name": "team_color", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0.0, "roughnessFactor": 1.0}},
    "fatigues":   {"name": "fatigues", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.0, "roughnessFactor": 1.0}},
    "skin":       {"name": "skin", "pbrMetallicRoughness": {"baseColorFactor": [0.87, 0.70, 0.55, 1], "metallicFactor": 0.0, "roughnessFactor": 1.0}},
    "gear":       {"name": "gear", "pbrMetallicRoughness": {"baseColorFactor": [0.16, 0.16, 0.18, 1], "metallicFactor": 0.0, "roughnessFactor": 1.0}},
}
MATERIAL_ORDER = ["team_color", "fatigues", "skin", "gear"]

# Face: normal, tangent u, tangent v with u x v = normal (so the corners below wind CCW from outside).
FACES = [
    ((1, 0, 0),  (0, 1, 0), (0, 0, 1)),
    ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
    ((0, 1, 0),  (0, 0, 1), (1, 0, 0)),
    ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
    ((0, 0, 1),  (1, 0, 0), (0, 1, 0)),
    ((0, 0, -1), (0, 1, 0), (1, 0, 0)),
]
CORNERS = [(-1, -1, 0, 0), (1, -1, 1, 0), (1, 1, 1, 1), (-1, 1, 0, 1)]   # su, sv, uv


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def box_geometry(centre, size):
    positions, normals, uvs, indices = [], [], [], []
    half = [s * 0.5 for s in size]
    for normal, u, v in FACES:
        assert cross(u, v) == normal
        base = len(positions)
        for su, sv, tu, tv in CORNERS:
            p = [centre[i] + half[i] * (normal[i] + su * u[i] + sv * v[i]) for i in range(3)]
            positions.append(tuple(p))
            normals.append(normal)
            uvs.append((tu, tv))
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    return positions, normals, uvs, indices


def camo_png(size=32, seed=7):
    rng = random.Random(seed)
    palette = [(84, 98, 62), (66, 78, 50), (108, 112, 78), (50, 58, 40)]
    pixels = [[palette[0]] * size for _ in range(size)]
    for _ in range(26):
        colour = rng.choice(palette)
        cx, cy, r = rng.randrange(size), rng.randrange(size), rng.randrange(3, 7)
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    pixels[y % size][x % size] = colour   # wraps, so it tiles
    raw = b"".join(b"\x00" + b"".join(bytes(px) for px in row) for row in pixels)

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def pad4(data, fill=b"\x00"):
    return data + fill * (-len(data) % 4)


# --- animation -------------------------------------------------------------
# A clip is (duration, {(joint, path): [(time, value), ...]}); rotations are xyzw quaternions.
# Rotating a hanging limb about +X by a negative angle swings it forward (+Z).

def qx(deg):
    a = math.radians(deg) * 0.5
    return (math.sin(a), 0.0, 0.0, math.cos(a))


def qy(deg):
    a = math.radians(deg) * 0.5
    return (0.0, math.sin(a), 0.0, math.cos(a))


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def cycle(duration, steps, fn):
    """Keys of a looping channel: fn(phase) sampled over one period, last key equal to the first."""
    return [(duration * i / steps, fn(2.0 * math.pi * i / steps)) for i in range(steps + 1)]


def hold(duration, value):
    return [(0.0, value), (duration, value)]


def idle_clip():
    T = 2.0
    s = math.sin
    return T, {
        ("hips", "translation"):   cycle(T, 8, lambda p: (0.0, 0.90 + 0.008 * s(p), 0.0)),
        ("spine", "rotation"):     cycle(T, 8, lambda p: qx(1.5 * s(p))),
        ("head", "rotation"):      cycle(T, 8, lambda p: qx(-1.0 * s(p))),
        ("shoulder_l", "rotation"): cycle(T, 8, lambda p: qx(2.0 * s(p))),
        ("shoulder_r", "rotation"): cycle(T, 8, lambda p: qx(-1.5 * s(p))),
    }


def walk_clip():
    T = 0.7
    s, c = math.sin, math.cos
    return T, {
        ("hips", "translation"):    cycle(T, 12, lambda p: (0.0, 0.90 + 0.03 * c(2 * p), 0.0)),
        ("spine", "rotation"):      cycle(T, 12, lambda p: qmul(qx(8.0), qy(5.0 * s(p)))),
        ("hip_l", "rotation"):      cycle(T, 12, lambda p: qx(-35.0 * s(p))),
        ("hip_r", "rotation"):      cycle(T, 12, lambda p: qx(35.0 * s(p))),
        ("knee_l", "rotation"):     cycle(T, 12, lambda p: qx(45.0 * max(0.0, c(p)))),
        ("knee_r", "rotation"):     cycle(T, 12, lambda p: qx(45.0 * max(0.0, -c(p)))),
        ("shoulder_l", "rotation"): cycle(T, 12, lambda p: qx(25.0 * s(p))),
        ("elbow_l", "rotation"):    hold(T, qx(-25.0)),
        ("shoulder_r", "rotation"): cycle(T, 12, lambda p: qx(-12.0 * s(p))),
        ("elbow_r", "rotation"):    hold(T, qx(-25.0)),
        # keeps the rifle level whatever the arm does
        ("weapon", "rotation"):     cycle(T, 12, lambda p: qx(25.0 + 12.0 * s(p))),
    }


def attack_clip():
    T = 1.0
    return T, {
        # aiming with the right arm, the muzzle kicking up at the start of each second
        ("shoulder_r", "rotation"): [(0.0, qx(-85.0)), (0.04, qx(-91.0)), (0.2, qx(-86.0)), (0.4, qx(-85.0)), (T, qx(-85.0))],
        ("elbow_r", "rotation"):    hold(T, qx(-5.0)),
        ("weapon", "rotation"):     hold(T, qx(90.0)),
        ("shoulder_l", "rotation"): hold(T, qx(-40.0)),
        ("elbow_l", "rotation"):    hold(T, qx(-60.0)),
        ("spine", "rotation"):      [(0.0, qx(0.0)), (0.04, qx(-3.0)), (0.3, qx(0.0)), (T, qx(0.0))],
        ("head", "rotation"):       hold(T, qx(0.0)),
    }


CLIPS = [("idle", idle_clip()), ("walk", walk_clip()), ("attack", attack_clip())]


def build():
    blob = bytearray()
    views, accessors, primitives = [], [], []

    def add_view(data, target=None):
        blob.extend(b"\x00" * (-len(blob) % 4))
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
        if target:
            view["target"] = target
        blob.extend(data)
        views.append(view)
        return len(views) - 1

    def add_accessor(view, component, count, kind, **extra):
        accessors.append({"bufferView": view, "componentType": component, "count": count, "type": kind, **extra})
        return len(accessors) - 1

    for material in MATERIAL_ORDER:
        positions, normals, uvs, indices, joints = [], [], [], [], []
        for name, joint, centre, size in BOXES:
            if name != material:
                continue
            p, n, t, i = box_geometry(centre, size)
            indices += [index + len(positions) for index in i]
            positions += p
            normals += n
            uvs += t
            joints += [JOINT_INDEX[joint]] * len(p)
        if not positions:
            continue
        lo = [min(p[i] for p in positions) for i in range(3)]
        hi = [max(p[i] for p in positions) for i in range(3)]
        pos = add_accessor(add_view(b"".join(struct.pack("<3f", *p) for p in positions), 34962), 5126, len(positions), "VEC3", min=lo, max=hi)
        nor = add_accessor(add_view(b"".join(struct.pack("<3f", *n) for n in normals), 34962), 5126, len(normals), "VEC3")
        tex = add_accessor(add_view(b"".join(struct.pack("<2f", *t) for t in uvs), 34962), 5126, len(uvs), "VEC2")
        jnt = add_accessor(add_view(b"".join(struct.pack("<4H", j, 0, 0, 0) for j in joints), 34962), 5123, len(joints), "VEC4")
        wgt = add_accessor(add_view(b"".join(struct.pack("<4f", 1.0, 0.0, 0.0, 0.0) for _ in joints), 34962), 5126, len(joints), "VEC4")
        idx = add_accessor(add_view(b"".join(struct.pack("<H", i) for i in indices), 34963), 5123, len(indices), "SCALAR")
        primitives.append({"attributes": {"POSITION": pos, "NORMAL": nor, "TEXCOORD_0": tex, "JOINTS_0": jnt, "WEIGHTS_0": wgt},
                           "indices": idx, "material": MATERIAL_ORDER.index(material), "mode": 4})

    # Skeleton: node 0 is the mesh, nodes 1.. are the joints in JOINTS order.
    nodes = [{"name": "soldier", "mesh": 0, "skin": 0}]
    for name, parent, position in JOINTS:
        local = position if parent is None else tuple(a - b for a, b in zip(position, JOINTS[JOINT_INDEX[parent]][2]))
        node = {"name": name, "translation": [round(v, 6) for v in local]}
        children = [1 + JOINT_INDEX[c] for c, p, _ in JOINTS if p == name]
        if children:
            node["children"] = children
        nodes.append(node)
    inverse_bind = b"".join(struct.pack("<16f", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -p[0], -p[1], -p[2], 1) for _, _, p in JOINTS)
    ibm = add_accessor(add_view(inverse_bind), 5126, len(JOINTS), "MAT4")
    skins = [{"joints": [1 + i for i in range(len(JOINTS))], "inverseBindMatrices": ibm, "skeleton": 1}]

    animations = []
    for clip_name, (duration, tracks) in CLIPS:
        samplers, channels = [], []
        for (joint, path), keys in tracks.items():
            times = [t for t, _ in keys]
            values = [v for _, v in keys]
            time_acc = add_accessor(add_view(b"".join(struct.pack("<f", t) for t in times)), 5126, len(times), "SCALAR",
                                    min=[min(times)], max=[max(times)])
            comps = 4 if path == "rotation" else 3
            value_acc = add_accessor(add_view(b"".join(struct.pack("<%df" % comps, *v) for v in values)), 5126, len(values),
                                     "VEC4" if comps == 4 else "VEC3")
            samplers.append({"input": time_acc, "output": value_acc, "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1, "target": {"node": 1 + JOINT_INDEX[joint], "path": path}})
        animations.append({"name": clip_name, "samplers": samplers, "channels": channels})

    image_view = add_view(camo_png())
    blob.extend(b"\x00" * (-len(blob) % 4))

    gltf = {
        "asset": {"version": "2.0", "generator": "joc_strategie make_soldier_glb.py"},
        "scene": 0,
        "scenes": [{"nodes": [0, 1]}],
        "nodes": nodes,
        "skins": skins,
        "animations": animations,
        "meshes": [{"name": "soldier", "primitives": primitives}],
        "materials": [MATERIALS[m] for m in MATERIAL_ORDER],
        "textures": [{"source": 0, "sampler": 0}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
        "images": [{"bufferView": image_view, "mimeType": "image/png"}],
        "buffers": [{"byteLength": len(blob)}],
        "bufferViews": views,
        "accessors": accessors,
    }
    json_chunk = pad4(json.dumps(gltf, separators=(",", ":")).encode("utf-8"), b" ")
    bin_chunk = pad4(bytes(blob))
    total = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
    return (struct.pack("<4sII", b"glTF", 2, total)
            + struct.pack("<I4s", len(json_chunk), b"JSON") + json_chunk
            + struct.pack("<I4s", len(bin_chunk), b"BIN\x00") + bin_chunk)


if __name__ == "__main__":
    out = os.path.normpath(OUT)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    data = build()
    with open(out, "wb") as f:
        f.write(data)
    print("wrote", out, len(data), "bytes")
    sys.exit(0)
