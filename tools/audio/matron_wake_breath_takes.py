#!/usr/bin/env python3
"""Prepare real CC0 takes for her wake and his breath (J-098, issue #416).

Runs offline at build time, never inside the game. Refuses changed source
bytes. No synthesizer, no pitch shift, no varispeed, no reused foreground
performance: each take is cut from a recorded performance. The wake's sub bed
is sine tones rendered offline (not a human sound) under the recorded gasp
and steel groan, the way the old synthesized wake laid its sub under its
breath; the game only plays the finished files.

Her wake (3.4 s a take, the old length, so the dread duck is unchanged):
a beat of hush, the tower's steel taking a load (a real iron groan), a wet
intake of breath (a real gasp), and the sub swell under them. Four takes:
four distinct gasps from one woman's gasping, four windows of one groan.

His breath: three in-halves and three out-halves cut from one man's
breathing (482831). In is the bright, sharp, falling burst (air rushing in
through the mouth); out is the dark, loud, rising burst (air pushed out).
If a listener's ear says the halves are swapped, it is a rename: the cuts
are per burst, nothing is re-cut. The picking folder holds two candidate
recordings per state (rest, climbing, winded) for Cory to pick by ear; until
he picks, the bank ships the rest set above.
"""
from __future__ import annotations

from array import array
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import urllib.request

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
MATRON = ROOT / "godot/presentation/audio/matron"
BREATH = ROOT / "godot/presentation/audio/breath"
PICKS = ROOT / "godot/presentation/audio/breath_wake_picks"
IMPORT = MATRON / "matron_breath_0.ogg.import"
MIX_RATE = 22050
PEAK = 0.88  # -1.1 dBFS, the recorded bank's ceiling

# id -> (uploader, URL to CC0 Freesound page, HQ preview URL, SHA256 of HQ preview).
SOURCES = {
    784717: ("superEGsonic", "https://freesound.org/people/superEGsonic/sounds/784717/",
             "https://cdn.freesound.org/previews/784/784717_663245-hq.mp3",
             "0f702e18c9dad7cef4f0130d5c1ef6d290527cc2d9d049ed6c5996af894f4410"),
    436781: ("Alivvie", "https://freesound.org/people/Alivvie/sounds/436781/",
             "https://cdn.freesound.org/previews/436/436781_4347097-hq.mp3",
             "e4d739f4a8986e592f097db93bda36a61c583526f8225a53dfa5e8262ac0015e"),
    253773: ("Reitanna", "https://freesound.org/people/Reitanna/sounds/253773/",
             "https://cdn.freesound.org/previews/253/253773_950925-hq.mp3",
             "7bf6cd1dfc7203bd12fcd68630532ce1f15b3240694fa651a22e80b2c93b3eef"),
    496836: ("hinchinbrook", "https://freesound.org/people/hinchinbrook/sounds/496836/",
             "https://cdn.freesound.org/previews/496/496836_9847211-hq.mp3",
             "6338c39f45ae7407ae4a388822a011e8849816549df4d74dbcd77662816a4f8c"),
    878048: ("AyaDrevis", "https://freesound.org/people/AyaDrevis/sounds/878048/",
             "https://cdn.freesound.org/previews/878/878048_14225658-hq.mp3",
             "47ec63d3dc3f725dc37219ac869a782451bebafd955ae065d950e9e819376934"),
    482831: ("craigsmith", "https://freesound.org/people/craigsmith/sounds/482831/",
             "https://cdn.freesound.org/previews/482/482831_2524442-hq.mp3",
             "6a49668416ccee72808c02e9eda89357907f7ec468fc7fe23eda730593524b3c"),
    386573: ("ShaneF91", "https://freesound.org/people/ShaneF91/sounds/386573/",
             "https://cdn.freesound.org/previews/386/386573_7233595-hq.mp3",
             "030b44e98cea76e4d0c4d7f8a296d82f372fd65624318b22d49b2b04991666f9"),
    844287: ("randbsoundbites", "https://freesound.org/people/randbsoundbites/sounds/844287/",
             "https://cdn.freesound.org/previews/844/844287_16968183-hq.mp3",
             "cc77685b322d654af0c0959f4d240f6c3ed0ed67509651c27b0a48cdcd198a2b"),
    456037: ("florianreichelt", "https://freesound.org/people/florianreichelt/sounds/456037/",
             "https://cdn.freesound.org/previews/456/456037_6253486-hq.mp3",
             "15317f9e78851cb9fd50f9154eb93570670b6ce6526bb72e1f8fe0c854e9bc4b"),
    874178: ("looplihood", "https://freesound.org/people/looplihood/sounds/874178/",
             "https://cdn.freesound.org/previews/874/874178_19375254-hq.mp3",
             "bdab1f23549c295df3f98265180b63102a87e1d0d33b9571d802b4b7413b4fee"),
    792641: ("Sadiquecat", "https://freesound.org/people/Sadiquecat/sounds/792641/",
             "https://cdn.freesound.org/previews/792/792641_5287430-hq.mp3",
             "b7dd0f6e5376473b7cefae30d9b22d6e64f75bf07160e28acad8e2ad5403ed51"),
}

# The shipped bank. (filename, source id, first second, last second.)
BREATH_TAKES = (
    ("breath_in_0.ogg", 482831, 6.90, 7.24),
    ("breath_in_1.ogg", 482831, 14.60, 14.94),
    ("breath_in_2.ogg", 482831, 19.70, 20.04),
    ("breath_out_0.ogg", 482831, 5.75, 6.17),
    ("breath_out_1.ogg", 482831, 13.40, 13.82),
    ("breath_out_2.ogg", 482831, 15.95, 16.37),
)

# The shipped wake takes: (filename, gasp id, gasp first, gasp last,
# bed id, bed first, bed last). Each take is 3.4 s: hush 0.30 s, then the
# bed (3.1 s) with the gasp (1.3 s) laid over it at 1.25 s, and the sub
# swell under both.
WAKE_TAKES = (
    ("matron_wake_0.ogg", 784717, 0.55, 1.85, 496836, 1.8, 4.9),
    ("matron_wake_1.ogg", 784717, 2.15, 3.45, 496836, 3.0, 6.1),
    ("matron_wake_2.ogg", 784717, 3.80, 5.10, 496836, 4.2, 7.3),
    ("matron_wake_3.ogg", 784717, 5.40, 6.70, 496836, 5.1, 8.2),
)

# The picking folder: alternate wake assemblies, then two candidate
# recordings per breath state. (filename, source id, first, last.)
WAKE_PICKS = (
    ("wake_alt_gasp_a.ogg", 436781, 0.30, 1.60, 496836, 1.8, 4.9),
    ("wake_alt_bed.ogg", 784717, 0.55, 1.85, 878048, 0.8, 3.9),
    ("wake_alt_gasp_b.ogg", 253773, 0.00, 0.60, 496836, 3.0, 6.1),
)
BREATH_PICKS = (
    ("breath_rest_a.ogg", 482831, 5.5, 9.0),
    ("breath_rest_b.ogg", 874178, 44.0, 48.0),
    ("breath_climb_a.ogg", 386573, 0.5, 4.0),
    ("breath_climb_b.ogg", 844287, 2.0, 6.0),
    ("breath_winded_a.ogg", 792641, 0.0, 2.38),
    ("breath_winded_b.ogg", 456037, 17.0, 20.5),
)


def run(*command: str, capture: bool = False) -> bytes:
    completed = subprocess.run(command, check=True, stdout=subprocess.PIPE if capture else None)
    return completed.stdout if capture else b""


def decode(path: Path, begin: float, duration: float) -> np.ndarray:
    """One channel of a source, 22,050 Hz float, cut to [begin, begin+duration)."""
    pcm = run("ffmpeg", "-hide_banner", "-loglevel", "error", "-ss", str(begin),
              "-i", str(path), "-t", str(duration), "-ac", "1", "-ar", str(MIX_RATE),
              "-f", "f32le", "-", capture=True)
    samples = np.frombuffer(pcm, dtype=np.float32).astype(np.float64)
    if samples.size == 0:
        raise RuntimeError(f"Silence/invalid performance: {path} {begin}-{begin + duration}")
    return samples


def highpass(samples: np.ndarray) -> np.ndarray:
    """The recorded bank's delivery-only high-pass, 75 Hz, one-pole."""
    alpha = 75.0 / (75.0 + MIX_RATE / (2.0 * np.pi))
    out = np.empty_like(samples)
    prev_in = 0.0
    prev_out = 0.0
    for i, value in enumerate(samples):
        prev_out = (1.0 - alpha) * (prev_out + value - prev_in)
        prev_in = value
        out[i] = prev_out
    return out


def finish(samples: np.ndarray) -> np.ndarray:
    """Peak-normalize to the bank's ceiling with short boundary fades."""
    peak = float(np.abs(samples).max())
    if peak < 0.001:
        raise RuntimeError("Silence/invalid performance: peak below -60 dBFS")
    samples = samples * (PEAK / peak)
    fade_in = int(0.006 * MIX_RATE)
    fade_out = int(0.04 * MIX_RATE)
    samples[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    samples[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)
    return np.clip(samples, -1.0, 1.0).astype(np.float32)


def encode_samples(samples: np.ndarray, output: Path) -> None:
    subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
                    "-f", "f32le", "-ar", str(MIX_RATE), "-ac", "1", "-i", "pipe:0",
                    "-c:a", "libvorbis", "-q:a", "5", str(output)],
                   input=np.ascontiguousarray(samples, dtype=np.float32).tobytes(), check=True)
    if output.stat().st_size < 1000:
        raise RuntimeError(f"Too little encoded audio: {output}")
    shutil.copyfile(IMPORT, output.with_suffix(".ogg.import"))
    got = float(run("ffprobe", "-v", "error", "-show_entries", "format=duration",
                    "-of", "default=nw=1:nk=1", str(output), capture=True).decode().strip())
    print(f"REAL_TAKE {output.name} duration={got:.3f}s "
          f"sha256={hashlib.sha256(output.read_bytes()).hexdigest()}", flush=True)


def cut_take(recordings: dict, filename: str, sid: int, begin: float, end: float,
             output: Path) -> None:
    duration = round(end - begin, 6)
    if not (0.12 <= duration <= 5.0):
        raise ValueError(f"Bad {filename} span: {duration}")
    samples = finish(highpass(decode(recordings[sid], begin, duration)))
    encode_samples(samples, output)


def wake_take(recordings: dict, filename: str, gasp_id: int, gasp_begin: float,
              gasp_end: float, bed_id: int, bed_begin: float, bed_end: float,
              output: Path) -> None:
    """One 3.4 s wake: hush, then the steel groan with the gasp over it
    at 1.25 s, and the sub swell (28/56/84 Hz) under both."""
    length = 3.4
    total = int(length * MIX_RATE)
    out = np.zeros(total)
    bed = decode(recordings[bed_id], bed_begin, bed_end - bed_begin)
    bed = highpass(bed)
    bed_start = int(0.30 * MIX_RATE)
    bed_end_at = min(total, bed_start + bed.size)
    bed_seg = bed[:bed_end_at - bed_start].copy()
    fade_in = int(0.5 * MIX_RATE)
    bed_seg[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    fade_out = int(0.6 * MIX_RATE)
    bed_seg[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)
    out[bed_start:bed_end_at] += bed_seg * 0.8
    gasp = decode(recordings[gasp_id], gasp_begin, gasp_end - gasp_begin)
    gasp = highpass(gasp)
    gasp_start = int(1.25 * MIX_RATE)
    gasp_end_at = min(total, gasp_start + gasp.size)
    gasp_seg = gasp[:gasp_end_at - gasp_start].copy()
    gasp_fade = int(0.05 * MIX_RATE)
    gasp_seg[:gasp_fade] *= np.linspace(0.0, 1.0, gasp_fade)
    gasp_seg[-gasp_fade:] *= np.linspace(1.0, 0.0, gasp_fade)
    out[gasp_start:gasp_end_at] += gasp_seg
    t = np.arange(total) / MIX_RATE
    sub_span = (t >= 0.30) & (t <= length)
    u = np.clip((t - 0.30) / (length - 0.30), 0.0, 1.0)
    env = np.minimum(1.0, u * 2.5) * np.sqrt(np.maximum(0.0, 1.0 - u))
    sub = (0.5 * np.sin(2 * np.pi * 28.0 * t)
           + 0.22 * np.sin(2 * np.pi * 56.0 * t)
           + 0.12 * np.sin(2 * np.pi * 84.0 * t)) * env * sub_span
    out += sub * 0.5
    encode_samples(finish(out), output)


def main() -> None:
    if not IMPORT.is_file():
        raise RuntimeError("Missing existing Ogg keep-import template")
    with tempfile.TemporaryDirectory(prefix="matron-wake-breath-") as scratch:
        recordings = {}
        for sid, (author, page, url, expected) in SOURCES.items():
            req = urllib.request.Request(url, headers={"User-Agent": "ScraperX-Recorded-Audio/1.0"})
            data = urllib.request.urlopen(req, timeout=60).read()
            digest = hashlib.sha256(data).hexdigest()
            if digest != expected:
                raise RuntimeError(f"Source {sid} changed; refuse to cut: {digest}")
            path = Path(scratch) / f"{sid}.mp3"
            path.write_bytes(data)
            recordings[sid] = path
            print(f"CC0_SOURCE {sid} {author} {page} sha256={digest}", flush=True)
        BREATH.mkdir(exist_ok=True)
        PICKS.mkdir(exist_ok=True)
        for filename, sid, begin, end in BREATH_TAKES:
            cut_take(recordings, filename, sid, begin, end, BREATH / filename)
        for row in WAKE_TAKES:
            wake_take(recordings, *row, MATRON / row[0])
        for filename, gasp_id, gasp_begin, gasp_end, bed_id, bed_begin, bed_end in WAKE_PICKS:
            wake_take(recordings, filename, gasp_id, gasp_begin, gasp_end,
                      bed_id, bed_begin, bed_end, PICKS / filename)
        for filename, sid, begin, end in BREATH_PICKS:
            cut_take(recordings, filename, sid, begin, end, PICKS / filename)
    print("MATRON_WAKE_BREATH_TAKES PASS 4 wake + 6 breath + 3 wake picks + 6 breath picks",
          flush=True)


if __name__ == "__main__":
    main()
