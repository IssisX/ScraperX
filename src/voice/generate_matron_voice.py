"""Generates her voice: the Matron's whole bank of spoken lines,
03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md (43 lines, three deliveries
each), made the way the climber's fear yells are (src/voice/generate_fall_voice.py).

Offline only. The game ships the Ogg Vorbis files this writes to
godot/presentation/audio/matron_voice/ and the table matron_voice_lines.gd;
nothing here runs on a device or in CI. It also writes CANDIDATES.md beside the
clips (voice, source, licence, and the settings, seed and Whisper transcript of
every clip) and generation_receipt.json, the same facts, written after every
clip: a run cut short carries on where it stopped when it is run again.

Her voice is all AI-generated, never a recording of a real person
(00_START_HERE.md rule 43). Source of every clip:
  * speech: Chatterbox TTS (Resemble AI, MIT licence), its `exaggeration`,
    `cfg_weight` and `temperature` giving each delivery (DELIVERIES);
  * voice identity: a reference sentence spoken by Piper's en_US-kristin-medium
    (trained from scratch on LibriVox public-domain recordings; the integrator's
    pick of 2026-10-29: American, and not Iona's voice base);
  * age: her voice is pushed older and rougher after the speech is made, with
    nothing added to it that is not already in the clip: pitched down AGE_SEMITONES,
    slowed to AGE_TEMPO, and a gentle saturation (AGE_DRIVE) for a rasp, the
    saturation the fall voice gives a yell. No noise is mixed in (Chatterbox
    makes no breath of its own, and code does not make one);
  * check: Whisper (OpenAI, MIT) transcribes each take as it ships (aged); a take
    is rejected unless the transcript holds at least 60% of its line's distinct
    words (generate_fall_voice.match), it ends within the line's time limit
    (generate_fall_voice.time_limit, stretched for a slow delivery), and it says
    at most a quarter more words than the line has. The best passing take ships.
    Every clip ships: one that no take passes fails the run, to be made again
    with --only and --takes.

Setup (a throwaway venv, CPU only; the same one generate_fall_voice.py uses):
  python3.12 -m venv tts
  tts/bin/pip install torch==2.6.0 torchaudio==2.6.0 --index-url https://download.pytorch.org/whl/cpu
  tts/bin/pip install chatterbox-tts==0.1.7 piper-tts==1.8.0 openai-whisper soundfile
  mkdir models && cd models
  B=https://huggingface.co/rhasspy/piper-voices/resolve/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd
  curl -LO $B/en/en_US/kristin/medium/en_US-kristin-medium.onnx \\
       -LO $B/en/en_US/kristin/medium/en_US-kristin-medium.onnx.json
Run (from the repository root):
  tts/bin/python src/voice/generate_matron_voice.py --models models
Make some clips again (the rest stay as they are):
  tts/bin/python src/voice/generate_matron_voice.py --models models --only matron_02_singsong --takes 12
Rewrite only matron_voice_lines.gd from the clips on disk:
  python3 src/voice/generate_matron_voice.py --table-only

Seeds are fixed per line, delivery and take, so a rerun on the same packages
and CPU reproduces the same selection.
"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# The fall voice's line matcher, time limit, silence trim and level/fade
# finish, imported as they are: that generator and its clips stay untouched.
import generate_fall_voice as fall  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
LINES_DOC = ROOT / "03_EXECUTION" / "MATRON_REDESIGN" / "HER_LINES_DRAFT.md"
OUT = ROOT / "godot" / "presentation" / "audio" / "matron_voice"

# The weights are pinned to the revision generate_story_voice.py uses.
CHATTERBOX = "ResembleAI/chatterbox"
CHATTERBOX_REVISION = "5bb1f6ee58e50c3b8d408bc82a6d3740c2db6e18"
WHISPER = "small.en"
PIPER_REPO = "rhasspy/piper-voices"
PIPER_REVISION = "5041a7a49ae5b3a0f30e6837769eb03fdbb231bd"
MIX_RATE = 22050
TAKES = 8

# Spoken by the Piper model to give Chatterbox the voice: calm, level talk,
# none of it a line of hers.
REFERENCE_TEXT = ("Good evening. The night staff have finished on the north side, and the cupboard "
                  "is locked at the second landing. Sit down when you are ready, and we will begin.")
VOICE = {
    "piper": "en_US-kristin-medium", "path": "en/en_US/kristin/medium",
    "who": "a woman, American",
    "dataset": "LibriVox recordings, about 11.5 hours", "dataset_url": "https://librivox.org",
    "dataset_licence": "public domain", "training": "trained from scratch",
}

# Older and rougher than the Piper voice and Chatterbox give. Pitch and tempo move the whole clip;
# the drive is tanh saturation, 1.0 being none.
AGE_SEMITONES = -2.0
AGE_TEMPO = 0.93
AGE_DRIVE = 1.5

# Delivery -> Chatterbox settings. Chatterbox's neutral exaggeration is 0.5 and cfg_weight 0.5; a lower
# cfg_weight slows the pace. `level_db` is added to the clip's voiced RMS (fall.finish levels it to
# -14 dB), and `slack` stretches the time limit for a slow delivery. What each is meant to give is a
# hope until the owner has heard it.
DELIVERIES = {
    # the sweet voice: high exaggeration, low cfg_weight
    "singsong": {"exaggeration": 0.85, "cfg_weight": 0.3, "temperature": 0.9, "level_db": 0.0, "slack": 2.0},
    # lips to the ear: the lowest exaggeration and cfg_weight, played quiet. A hush, not a whisper:
    # Chatterbox has no unvoiced speech.
    "hushed": {"exaggeration": 0.25, "cfg_weight": 0.2, "temperature": 0.7, "level_db": -9.0, "slack": 2.2},
    # the mask off: the highest exaggeration
    "snarl": {"exaggeration": 1.1, "cfg_weight": 0.4, "temperature": 0.85, "level_db": 0.0, "slack": 1.6},
    # no lift, no hurry: low exaggeration and temperature, full voice
    "menace": {"exaggeration": 0.3, "cfg_weight": 0.35, "temperature": 0.6, "level_db": 0.0, "slack": 1.8},
}

# The moment each line belongs to (the sections of HER_LINES_DRAFT.md) -> the three deliveries it is
# made in. A moment draws a clip of the line it is playing without repeating the last (rule 42).
MOMENTS = {
    "hunting": ((1, 2, 3, 4, 5, 6, 41, 42, 43), ("singsong", "hushed", "menace")),
    "sees": ((7, 8, 9, 10, 28, 29), ("menace", "snarl", "singsong")),
    "grab": ((11, 12, 13, 14, 30, 31), ("snarl", "menace", "singsong")),
    "drop": ((15, 16, 17, 18, 32, 33, 34), ("menace", "hushed", "snarl")),
    "rip": ((19, 20, 21, 35, 36), ("singsong", "menace", "snarl")),
    "free": ((22, 23, 24, 37, 38), ("snarl", "menace", "singsong")),
    "lost": ((25, 26, 39, 40), ("hushed", "singsong", "menace")),
    "last": ((27,), ("hushed", "menace", "singsong")),
}


def draft_lines():
    """Line number -> the words she says, read from the owner-approved draft so
    there is one text of each line: `27. *(stage direction)* "Waste not."`
    gives Waste not. (a stage direction and *emphasis* are not spoken)."""
    found = {}
    for row in LINES_DOC.read_text().splitlines():
        hit = re.match(r'(\d+)\.\s+(?:\*\(.*?\)\*\s+)?"(.*)"\s*$', row)
        if hit:
            found[int(hit.group(1))] = hit.group(2).replace('\\"', '"').replace("*", "")
    return found


def bank():
    """(line number, moment, delivery) for every clip, in line order."""
    items = []
    for moment, (numbers, deliveries) in MOMENTS.items():
        for number in numbers:
            items += [(number, moment, delivery) for delivery in deliveries]
    return sorted(items)


def clip_name(line_no, delivery):
    return f"matron_{line_no:02d}_{delivery}"


def seed_of(line_no, delivery, take):
    return zlib.crc32(f"kristin|{line_no}|{delivery}|{take}".encode())


def age(samples, rate):
    """Her voice older and rougher: down in pitch, slower, a gentle saturation. Nothing is mixed in."""
    import librosa
    import numpy as np
    out = librosa.effects.pitch_shift(samples.astype(np.float32), sr=rate, n_steps=AGE_SEMITONES)
    out = librosa.effects.time_stretch(out, rate=AGE_TEMPO)
    peak = float(np.abs(out).max()) + 1e-9
    out = np.tanh(out / peak * AGE_DRIVE) / np.tanh(AGE_DRIVE) * peak
    return fall.trim(out.astype(np.float32), rate)


def finish(samples, delivery):
    return fall.finish(samples, MIX_RATE, "") * float(10.0 ** (DELIVERIES[delivery]["level_db"] / 20.0))


def gd_string(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def write_table(out, rows):
    """matron_voice_lines.gd: moment -> line number -> its clips; line number -> its caption."""
    text = draft_lines()
    present = {row["name"] for row in rows if (out / f"{row['name']}.ogg").exists()}
    with open(out.parent / "matron_voice_lines.gd", "w") as gd:
        gd.write("extends RefCounted\n")
        gd.write("# Written by src/voice/generate_matron_voice.py -- do not edit by hand.\n")
        gd.write("# Moment -> line number -> its clip files in presentation/audio/matron_voice/;\n")
        gd.write("# line number -> the words. Her last word, at the apron wipe, is LAST_LINE.\n\n")
        gd.write("const DIR := \"res://presentation/audio/matron_voice/\"\n")
        gd.write("const LAST_LINE := 27\n")
        gd.write("const MOMENTS := {\n")
        for moment, (numbers, deliveries) in MOMENTS.items():
            gd.write(f"\t&\"{moment}\": {{\n")
            for number in numbers:
                clips = [clip_name(number, d) + ".ogg" for d in deliveries if clip_name(number, d) in present]
                gd.write(f"\t\t{number}: [" + ", ".join(gd_string(c) for c in clips) + "],\n")
            gd.write("\t},\n")
        gd.write("}\n")
        gd.write("const CAPTIONS := {\n")
        for number in sorted(text):
            gd.write(f"\t{number}: {gd_string(text[number])},\n")
        gd.write("}\n")


def write_candidates(out, made):
    rows = [
        "# Her voice: the bank",
        "",
        "Written by src/voice/generate_matron_voice.py. Each clip is Chatterbox TTS (MIT licence) speaking one",
        "line from 03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md in the voice of a reference sentence spoken by",
        "a Piper voice (an AI voice, never a recording of a real person), made older and rougher after the speech",
        "(pitch %+.1f semitones, tempo %.2f, saturation drive %.1f; nothing mixed in). Whisper checked that the clip as"
        % (AGE_SEMITONES, AGE_TEMPO, AGE_DRIVE),
        "shipped says the line. Three deliveries of every line, so no line repeats soon.",
        "",
        "## Voice",
        "",
        f"- Piper {VOICE['piper']} (https://huggingface.co/{PIPER_REPO}/tree/{PIPER_REVISION}/{VOICE['path']}), "
        f"{VOICE['training']} on {VOICE['dataset']} ({VOICE['dataset_url']}), licence: {VOICE['dataset_licence']}",
        "- speech: Chatterbox TTS 0.1.7, ResembleAI/chatterbox @ %s (MIT licence)" % CHATTERBOX_REVISION,
        "- check: openai-whisper %s" % WHISPER,
        "",
        "## Clips",
        "",
        "| clip | line | delivery | exaggeration | cfg_weight | temperature | level dB | seed (take) | seconds | match | Whisper heard |",
        "|---|---|---|---|---|---|---|---|---|---|---|",
    ]
    for item in made:
        d = DELIVERIES[item["delivery"]]
        rows.append(f"| {item['name']}.ogg | {item['line_no']} {item['line']!r} | {item['delivery']} "
                    f"| {d['exaggeration']} | {d['cfg_weight']} | {d['temperature']} | {-14.0 + d['level_db']:.0f} "
                    f"| {item['seed']} ({item['take']}) | {item['seconds']:.2f} | {item['match']:.2f} | {item['heard']!r} |")
    rows += ["", "Deliveries are what the settings are meant to give, not yet heard by anyone: `singsong` the sweet",
             "voice; `hushed` lips to the ear (a hush played quiet, not an unvoiced whisper: Chatterbox has none);",
             "`snarl` the mask off; `menace` no lift, no hurry.", ""]
    (out / "CANDIDATES.md").write_text("\n".join(rows))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(OUT))
    parser.add_argument("--models", help="the folder holding the Piper .onnx files")
    parser.add_argument("--work", default="voice_work", help="where the reference sentence is written")
    parser.add_argument("--takes", type=int, default=TAKES)
    parser.add_argument("--only", default="",
                        help="make only these clips again, by name, e.g. matron_02_singsong,matron_27_hushed")
    parser.add_argument("--table-only", action="store_true",
                        help="rewrite matron_voice_lines.gd from the receipt and clips on disk; generate nothing")
    args = parser.parse_args()
    out = Path(args.out)
    only = {item for item in args.only.split(",") if item}
    text = draft_lines()
    receipt = out / "generation_receipt.json"
    rows = {item["name"]: item for item in json.loads(receipt.read_text())} if receipt.exists() else {}
    if sorted({n for n, _, _ in bank()}) != sorted(text):
        sys.exit(f"{LINES_DOC} and MOMENTS disagree about the lines")
    if args.table_only:
        write_table(out, [rows[key] for key in sorted(rows)])
        return
    if not args.models:
        parser.error("--models is required to generate")

    import librosa
    import numpy as np
    import soundfile as sf
    import torch
    import whisper
    from chatterbox.tts import ChatterboxTTS
    from huggingface_hub import hf_hub_download

    # from_pretrained() takes whatever the model repo holds today; the weights
    # are pinned to CHATTERBOX_REVISION instead.
    for name in ("ve.safetensors", "t3_cfg.safetensors", "s3gen.safetensors", "tokenizer.json", "conds.pt"):
        weights = hf_hub_download(repo_id=CHATTERBOX, filename=name, revision=CHATTERBOX_REVISION)
    tts = ChatterboxTTS.from_local(Path(weights).parent, "cpu")
    # generate() asks for up to 1000 speech tokens (40 s at 25 tokens/s); a
    # take that babbles is cut at 1.5x its line's time limit instead.
    inference = tts.t3.inference
    budget = {"tokens": 1000}

    def capped(*a, **k):
        k["max_new_tokens"] = budget["tokens"]
        return inference(*a, **k)

    tts.t3.inference = capped
    asr = whisper.load_model(WHISPER)

    out.mkdir(parents=True, exist_ok=True)
    work = Path(args.work)
    work.mkdir(parents=True, exist_ok=True)
    reference = work / "reference_kristin.wav"
    subprocess.run([os.path.join(os.path.dirname(sys.executable), "piper"), "-m",
                    str(Path(args.models) / f"{VOICE['piper']}.onnx"), "-f", str(reference)],
                   input=REFERENCE_TEXT.encode(), check=True)
    rejected = []
    for line_no, moment, delivery in bank():
        line = text[line_no]
        name = clip_name(line_no, delivery)
        if only and name not in only:
            continue
        if not only and name in rows and (out / f"{name}.ogg").exists():
            continue
        settings = DELIVERIES[delivery]
        # The aged clip runs 1/AGE_TEMPO longer than the speech made.
        limit = fall.time_limit(line) * settings["slack"] / AGE_TEMPO
        budget["tokens"] = int(25 * 1.5 * limit * AGE_TEMPO)
        best = None
        for take in range(args.takes):
            seed = seed_of(line_no, delivery, take)
            torch.manual_seed(seed)
            wav = tts.generate(line, audio_prompt_path=str(reference),
                               exaggeration=settings["exaggeration"], cfg_weight=settings["cfg_weight"],
                               temperature=settings["temperature"])
            raw = wav.squeeze(0).numpy()
            clip = age(fall.trim(librosa.resample(raw, orig_sr=tts.sr, target_sr=MIX_RATE), MIX_RATE), MIX_RATE)
            heard = asr.transcribe(librosa.resample(clip, orig_sr=MIX_RATE, target_sr=16000)
                                   .astype(np.float32), fp16=False, temperature=0.0)["text"].strip()
            score = fall.match(line, heard)
            seconds = len(clip) / MIX_RATE
            said, wanted = len(fall.words(heard)), len(fall.words(line))
            extra = said - wanted
            # A take that says words of its own is as wrong as one that babbles: at most a quarter
            # more words than the line has.
            ok = score >= fall.MIN_MATCH and seconds <= limit and said <= wanted + wanted // 4
            print(f"{name} take {take}: {seconds:.2f}s match {score:.2f} {'ok' if ok else 'REJECT'} {heard!r}",
                  flush=True)
            if ok and (best is None or (score, -abs(extra), -seconds) >
                       (best["match"], -abs(best["extra"]), -best["seconds"])):
                best = {"name": name, "moment": moment, "line_no": line_no, "line": line,
                        "delivery": delivery, "seed": seed, "take": take, "seconds": seconds,
                        "match": score, "extra": extra, "heard": heard, "clip": clip}
            if best is not None and best["match"] >= 1.0 and best["extra"] == 0:
                break
        if best is None:
            rejected.append(f"{name} {line!r}")
            continue
        sf.write(out / f"{name}.ogg", finish(best["clip"], delivery), MIX_RATE, format="OGG", subtype="VORBIS")
        (out / f"{name}.ogg.import").write_text('[remap]\n\nimporter="keep"\n')
        rows[name] = {k: v for k, v in best.items() if k != "clip"}
        done = [rows[key] for key in sorted(rows)]
        receipt.write_text(json.dumps(done, indent=2) + "\n")
        write_candidates(out, done)
        write_table(out, done)

    if rejected:
        print("NO PASSING TAKE (run again with --only and more --takes):\n  " + "\n  ".join(rejected))
        sys.exit(1)


if __name__ == "__main__":
    main()
