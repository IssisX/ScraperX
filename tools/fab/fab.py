#!/usr/bin/env python3
"""The fabrication tool: any GLB in, the game's fabricated object out
(03_EXECUTION/LIVE_FABRICATION/DESIGN.md, build step 1). No server, no
network: a model on disk becomes, in an output folder,

    NAME.glb    the model scaled to the requested size (never more than the
                dispatch carrier's 1.2 m), cut to about 4,000 triangles and
                one 512 px texture
    NAME.hull   the hull file the native simulation builds the body from:
                at most 8 convex hulls of at most 32 points, the material and
                the mass (the hulls' volume times the material's density),
                bound to the GLB by its SHA-512 (hullfile.py has the layout)
    NAME.sig    an Ed25519 signature of the hull file (SIGNED_PREFIX first),
                by the dev key; the game checks it with the public key in
                the repository (dev_public_key.hex) before it reads anything
    NAME.json   what was made, for people: sizes, counts, hashes, the source

Setting up (pinned, hashes checked; licenses in third_party/README.md):

    python3 -m venv build/fab-venv
    build/fab-venv/bin/pip install --require-hashes -r tools/fab/requirements.txt

Then, always with -I (no user site, no current directory on the path):

    build/fab-venv/bin/python3 -I tools/fab/fab.py keygen
    build/fab-venv/bin/python3 -I tools/fab/fab.py make MODEL.glb --name crowbar \\
        --material steel --size 0.6 --out OUT_DIR
    build/fab-venv/bin/python3 -I tools/fab/fab.py verify OUT_DIR/crowbar

keygen makes the dev key pair: the private key outside the repository
(DEFAULT_PRIVATE, refused anywhere inside a git work tree, so it is never
committed) and the public key into dev_public_key.hex, which the game's
build compiles in. A new key pair makes every object signed by the old one
fail its check, by design.
"""
import argparse
import hashlib
import json
import os
import stat
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import fabricate  # noqa: E402
import glb  # noqa: E402
import hullfile  # noqa: E402

DEFAULT_PRIVATE = os.path.join(os.path.expanduser("~"), ".config", "scraperx", "fab_dev_private_key.pem")
DEFAULT_PUBLIC = os.path.join(HERE, "dev_public_key.hex")
# Signed ahead of the hull file, so a signature made for anything else never
# passes for a hull file (src/bridge/fabricated_delivery.cpp holds the same).
SIGNED_PREFIX = b"ScraperX fabricated object v1\n"
# The most a delivery may download (DESIGN.md: "downloads at most 2 MB").
MAX_GLB_BYTES = 2 * 1024 * 1024


def _inside_git_tree(path):
    folder = os.path.dirname(os.path.abspath(path))
    while True:
        if os.path.exists(os.path.join(folder, ".git")):
            return True
        parent = os.path.dirname(folder)
        if parent == folder:
            return False
        folder = parent


def _load_private(path):
    from cryptography.hazmat.primitives import serialization
    with open(path, "rb") as handle:
        return serialization.load_pem_private_key(handle.read(), password=None)


def _public_hex(private_key):
    from cryptography.hazmat.primitives import serialization
    raw = private_key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    return raw.hex()


def sign_message(hull_bytes):
    return SIGNED_PREFIX + hull_bytes


def keygen(args):
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    if _inside_git_tree(args.private):
        sys.exit("refused: %s is inside a git work tree; the private key never goes in one" % args.private)
    if os.path.exists(args.private) and not args.force:
        sys.exit("refused: %s exists (--force replaces it, and every object signed with it then fails)"
                 % args.private)
    key = Ed25519PrivateKey.generate()
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                            serialization.NoEncryption())
    os.makedirs(os.path.dirname(args.private), exist_ok=True)
    fd = os.open(args.private, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, stat.S_IRUSR | stat.S_IWUSR)
    with os.fdopen(fd, "wb") as handle:
        handle.write(pem)
    with open(args.public, "w") as handle:
        handle.write(_public_hex(key) + "\n")
    print("private key: %s (kept out of every repository)" % args.private)
    print("public key:  %s  %s" % (args.public, _public_hex(key)))


def make(args):
    key = _load_private(args.private)
    public_hex = open(args.public).read().strip()
    if _public_hex(key) != public_hex:
        sys.exit("refused: the private key at %s is not the pair of %s" % (args.private, args.public))
    with open(args.model, "rb") as handle:
        data = handle.read()
    try:
        parts = glb.read_parts(data)
        fitted = fabricate.fit(parts, args.size, args.material, args.triangles, args.texture, args.seed)
    except (glb.GlbError, fabricate.FitError) as error:
        sys.exit("refused: %s" % error)
    model = glb.write_glb(fitted.positions, fitted.normals, fitted.uvs, fitted.triangles, fitted.texture,
                         "image/jpeg", fitted.look, args.name)
    if len(model) > MAX_GLB_BYTES:
        sys.exit("refused: the fitted GLB is %d bytes, over %d" % (len(model), MAX_GLB_BYTES))
    glb_digest = hashlib.sha512(model).digest()
    hull = hullfile.pack(args.material, fitted.mass_kg, fitted.volume_m3, fitted.half_extent, glb_digest,
                         [[tuple(p) for p in h] for h in fitted.hulls])
    signature = key.sign(sign_message(hull))
    os.makedirs(args.out, exist_ok=True)
    stem = os.path.join(args.out, args.name)
    for suffix, blob in ((".glb", model), (".hull", hull), (".sig", signature)):
        with open(stem + suffix, "wb") as handle:
            handle.write(blob)
    record = {
        "name": args.name,
        "source": {"file": os.path.basename(args.model), "sha256": hashlib.sha256(data).hexdigest(),
                   "triangles": fitted.source_triangles,
                   "size_m": [round(float(x), 4) for x in fitted.source_span]},
        "material": args.material,
        "density_kg_m3": fitted.density,
        "size_m": round(fitted.size_m, 4),
        "half_extent_m": [round(x, 4) for x in fitted.half_extent],
        "triangles": int(len(fitted.triangles)),
        "texture_px": fitted.texture_px,
        "texture_coverage": round(fitted.texture_coverage, 3),
        "hulls": [len(h) for h in fitted.hulls],
        "hull_volumes_m3": [round(float(v), 7) for v in fitted.hull_volumes],
        "volume_m3": round(fitted.volume_m3, 7),
        "mass_kg": round(fitted.mass_kg, 3),
        "glb_bytes": len(model),
        "glb_sha512": glb_digest.hex(),
        "hull_bytes": len(hull),
        "hull_sha512": hashlib.sha512(hull).hexdigest(),
        "public_key": public_hex,
        "settings": {"triangles": args.triangles, "texture": args.texture, "seed": args.seed},
    }
    with open(stem + ".json", "w") as handle:
        json.dump(record, handle, indent=1, sort_keys=True)
        handle.write("\n")
    print("made %s: %d triangles, %d px texture, %d hulls (%s points), %.4f m3 of %s, %.2f kg, %d bytes GLB"
          % (args.name, len(fitted.triangles), fitted.texture_px, len(fitted.hulls),
             "/".join(str(len(h)) for h in fitted.hulls), fitted.volume_m3, args.material, fitted.mass_kg,
             len(model)))


def verify(args):
    """The checks the game makes, here for people: the signature, the GLB's
    hash, and the hull file's bounds."""
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
    stem = args.object
    hull = open(stem + ".hull", "rb").read()
    signature = open(stem + ".sig", "rb").read()
    model = open(stem + ".glb", "rb").read()
    public = Ed25519PublicKey.from_public_bytes(bytes.fromhex(open(args.public).read().strip()))
    try:
        public.verify(signature, sign_message(hull))
    except InvalidSignature:
        sys.exit("FAIL %s: the signature does not verify" % stem)
    fields = hullfile.unpack(hull)
    if hashlib.sha512(model).digest() != fields["glb_sha512"]:
        sys.exit("FAIL %s: the GLB is not the one the hull file names" % stem)
    print("OK %s: %s, %.2f kg, %d hulls, signed by %s" % (stem, fields["material"], fields["mass_kg"],
                                                          len(fields["hulls"]), open(args.public).read().strip()))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("keygen", help="make the dev key pair")
    p.add_argument("--private", default=DEFAULT_PRIVATE)
    p.add_argument("--public", default=DEFAULT_PUBLIC)
    p.add_argument("--force", action="store_true")
    p.set_defaults(run=keygen)
    p = sub.add_parser("make", help="fit a GLB, decompose, weigh and sign it")
    p.add_argument("model")
    p.add_argument("--name", required=True)
    p.add_argument("--material", required=True, choices=sorted(hullfile.MATERIALS))
    p.add_argument("--size", type=float, required=True, help="its longest side, m (at most 1.2)")
    p.add_argument("--out", default=".")
    p.add_argument("--private", default=DEFAULT_PRIVATE)
    p.add_argument("--public", default=DEFAULT_PUBLIC)
    p.add_argument("--triangles", type=int, default=4000)
    p.add_argument("--texture", type=int, default=512)
    p.add_argument("--seed", type=int, default=0)
    p.set_defaults(run=make)
    p = sub.add_parser("verify", help="check an object as the game does")
    p.add_argument("object", help="the output stem, e.g. OUT_DIR/crowbar")
    p.add_argument("--public", default=DEFAULT_PUBLIC)
    p.set_defaults(run=verify)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
