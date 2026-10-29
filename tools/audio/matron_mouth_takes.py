#!/usr/bin/env python3
"""Prepare authentic CC0 Matron mouth takes from specific recorded performances.

Runs offline at build time, never inside the game. Refuses changed source bytes.
No synthesizer, effects layering, pitch shift, varispeed, or reused foreground
performance. Each take is an isolated, nonoverlapping natural source event.
"""
from __future__ import annotations

from array import array
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
BANK = ROOT / "godot/presentation/audio/matron"
IMPORT = BANK / "matron_breath_0.ogg.import"

# id -> (uploader, URL to CC0 Freesound page, HQ preview URL, SHA256 of HQ preview).
SOURCES = {
    711535: ("Vein_Adams", "https://freesound.org/people/Vein_Adams/sounds/711535/",
             "https://cdn.freesound.org/previews/711/711535_14535773-hq.ogg",
             "942f21d4be754d3de11bedc5dac21f4862ff0566b30208a133e2025a391fd509"),
    842294: ("CreepyKing", "https://freesound.org/people/CreepyKing/sounds/842294/",
             "https://cdn.freesound.org/previews/842/842294_14303554-hq.ogg",
             "f0a0d6484da09adf0c13c8bf28680f3446d476c009bb69d37cbc4328f4032120"),
    586979: ("RavenWolfProds", "https://freesound.org/people/RavenWolfProds/sounds/586979/",
             "https://cdn.freesound.org/previews/586/586979_9619518-hq.ogg",
             "b23171c6d424784814e11f0500a4125190a2c479c69acf46340217989a19dcbd"),
    516473: ("padpad", "https://freesound.org/people/padpad/sounds/516473/",
             "https://cdn.freesound.org/previews/516/516473_7345129-hq.ogg",
             "433d804d1f636bdcd2eb19dbe68cacedc473fa383915a13b9d83226a7c81f248"),
    151218: ("OwlStorm", "https://freesound.org/people/OwlStorm/sounds/151218/",
             "https://cdn.freesound.org/previews/151/151218_140737-hq.ogg",
             "f4af92a89ba8e712964e59e9b9e065958c294792817fc4177acde821b478da28"),
}

# filename, source id, first second, last second.
# Independent throat-clearing efforts: RavenWolfProds (0, 1, 2);
# CreepyKing (3). Distinct human tongue clicks: Vein_Adams (0..3).
# Three separate sniff performances by padpad; fourth by OwlStorm.
TAKES = (
    ("matron_croak_0.ogg", 586979, 0.38, 1.08),
    ("matron_croak_1.ogg", 586979, 10.50, 11.18),
    ("matron_croak_2.ogg", 586979, 22.32, 23.45),
    ("matron_croak_3.ogg", 842294, 0.16, 1.17),
    ("matron_cluck_0.ogg", 711535, 0.14, 0.39),
    ("matron_cluck_1.ogg", 711535, 0.92, 1.20),
    ("matron_cluck_2.ogg", 711535, 1.73, 2.00),
    ("matron_cluck_3.ogg", 711535, 4.03, 4.36),
    ("matron_sniff_0.ogg", 516473, 0.62, 1.33),
    ("matron_sniff_1.ogg", 516473, 2.51, 3.35),
    ("matron_sniff_2.ogg", 516473, 4.35, 5.20),
    ("matron_sniff_3.ogg", 151218, 0.05, 0.59),
)

def run(*command: str, capture: bool = False) -> bytes:
    completed = subprocess.run(command, check=True, stdout=subprocess.PIPE if capture else None)
    return completed.stdout if capture else b""

def main() -> None:
    if not IMPORT.is_file():
        raise RuntimeError("Missing existing Ogg keep-import template")
    with tempfile.TemporaryDirectory(prefix="matron-cc0-") as scratch:
        recordings = {}
        for sid in {row[1] for row in TAKES}:
            author, page, url, expected = SOURCES[sid]
            req = urllib.request.Request(url, headers={"User-Agent":"ScraperX-Recorded-Audio/1.0"})
            data = urllib.request.urlopen(req, timeout=40).read()
            digest = hashlib.sha256(data).hexdigest()
            if digest != expected:
                raise RuntimeError(f"Source {sid} changed; refuse to cut: {digest}")
            path = Path(scratch) / f"{sid}.ogg"
            path.write_bytes(data)
            recordings[sid] = path
            print(f"CC0_SOURCE {sid} {author} {page} sha256={digest}", flush=True)
        BANK.mkdir(exist_ok=True)
        for filename, sid, begin, end in TAKES:
            duration = round(end - begin, 6)
            if not (0.12 <= duration <= 1.2):
                raise ValueError(f"Bad {filename} span: {duration}")
            source = str(recordings[sid])
            # Measure the real recorded peak, not a synthetic or resynthesized layer.
            pcm = run("ffmpeg","-hide_banner","-loglevel","error","-ss",str(begin),
                      "-i",source,"-t",str(duration),"-ac","1","-ar","22050",
                      "-f","f32le","-",capture=True)
            samples = array("f")
            samples.frombytes(pcm)
            peak = max((abs(x) for x in samples),default=0)
            if peak < 0.001:
                raise RuntimeError(f"Silence/invalid performance: {filename}")
            gain = min(0.88/peak, 32.0)
            end_fade = max(0.0, duration - 0.04)
            # Delivery-only HPF, short boundary fades, gain alignment and mono/22k
            # encoding, as with the existing recorded bank. Rate stays 1.0.
            filters = (f"highpass=f=75,volume={gain:.7f},"
                       f"afade=t=in:st=0:d=0.006,"
                       f"afade=t=out:st={end_fade:.4f}:d=0.04")
            output = BANK / filename
            run("ffmpeg","-y","-hide_banner","-loglevel","error",
                "-ss",str(begin),"-i",source,"-t",str(duration),
                "-vn","-af",filters,"-ar","22050","-ac","1",
                "-c:a","libvorbis","-q:a","5",str(output))
            if output.stat().st_size < 1000:
                raise RuntimeError(f"Too little encoded audio: {filename}")
            got = float(run("ffprobe","-v","error","-show_entries","format=duration",
                            "-of","default=nw=1:nk=1",str(output),capture=True).decode().strip())
            if not 0.1 <= got <= 1.2:
                raise RuntimeError(f"Outside take length 1.2s: {filename} {got}")
            shutil.copyfile(IMPORT,BANK/(filename+".import"))
            print(f"REAL_TAKE {filename} source={sid} {begin:.2f}-{end:.2f}s "
                  f"duration={got:.3f}s peak={peak:.3f} gain={gain:.2f} "
                  f"sha256={hashlib.sha256(output.read_bytes()).hexdigest()}",flush=True)
    print("MAT_RON_CC0_TAKES PASS 4 croak 4 cluck 4 sniff",flush=True)

if __name__ == "__main__":
    main()
