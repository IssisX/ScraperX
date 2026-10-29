"""Generates her voice: the Matron's spoken lines, 03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md,
made the way the climber's fear yells are (src/voice/generate_fall_voice.py).

Offline only. The game would ship the Ogg Vorbis files this writes to
godot/presentation/audio/matron_voice/; nothing here runs on a device or in
CI. For now it makes candidates only: a few lines (CANDIDATES) in each of the
reference voices (VOICES), so the owner picks her voice by ear before the full
bank is made. It writes CANDIDATES.md beside the clips (voice, source,
licence, and the settings, seed and Whisper transcript of every clip) and
generation_receipt.json, the same facts, written after every clip: a run cut
short carries on where it stopped when it is run again.

Source of every clip:
  * speech: Chatterbox TTS (Resemble AI, MIT licence), its `exaggeration`,
    `cfg_weight` and `temperature` giving each delivery (DELIVERIES). No other
    synthesis, and no filter that makes a voice: a clip is levelled and faded,
    nothing more;
  * voice identity: a reference sentence spoken by a Piper voice trained from
    scratch on public-domain recordings (VOICES, each with its dataset and
    licence from its model card). Her voice is all AI-generated, never a
    recording of a real person (00_START_HERE.md rule 43, 2026-10-29);
  * check: Whisper (OpenAI, MIT) transcribes each take; a take is rejected
    unless the transcript holds at least 60% of its line's distinct words
    (generate_fall_voice.match), it ends within the line's time limit
    (generate_fall_voice.time_limit, stretched for a slow delivery), and it says
    at most a quarter more words than the line has, so no clip ships that says
    something else, nothing at all, or babbles on. The best passing take
    ships. Every clip ships: one that no take passes fails the run, to be
    made again with --only and --takes.

Setup (a throwaway venv, CPU only; the same one generate_fall_voice.py uses):
  python3.12 -m venv tts
  tts/bin/pip install torch==2.6.0 torchaudio==2.6.0 --index-url https://download.pytorch.org/whl/cpu
  tts/bin/pip install chatterbox-tts==0.1.7 piper-tts==1.8.0 openai-whisper soundfile
  mkdir models && cd models
  B=https://huggingface.co/rhasspy/piper-voices/resolve/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd
  for v in en/en_US/kristin/medium/en_US-kristin-medium en/en_GB/cori/high/en_GB-cori-high \\
           en/en_US/ljspeech/high/en_US-ljspeech-high; do
    curl -LO $B/$v.onnx -LO $B/$v.onnx.json; done
Run (from the repository root):
  tts/bin/python src/voice/generate_matron_voice.py --models models
Make some clips again (the rest stay as they are):
  tts/bin/python src/voice/generate_matron_voice.py --models models --only matron_02_kristin_singsong --takes 12

Seeds are fixed per voice, line, delivery and take, so a rerun on the same
packages and CPU reproduces the same selection.
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

# Spoken by every voice's Piper model to give Chatterbox the voice: calm, level
# talk, none of it a line of hers.
REFERENCE_TEXT = ("Good evening. The night staff have finished on the north side, and the cupboard "
                  "is locked at the second landing. Sit down when you are ready, and we will begin.")

# Voice name -> the Piper voice that gives it its identity. Each model card's
# dataset and licence, read at PIPER_REVISION, are written down with it. All
# three are trained from scratch (none is fine-tuned from en_US-lessac, whose
# Blizzard 2013 data is not cleared for a commercial game). en_US-ljspeech is
# also the story radio's Iona (generate_story_voice.py).
VOICES = {
    "kristin": {
        "piper": "en_US-kristin-medium", "path": "en/en_US/kristin/medium",
        "who": "a woman, American",
        "dataset": "LibriVox recordings, about 11.5 hours", "dataset_url": "https://librivox.org",
        "dataset_licence": "public domain", "training": "trained from scratch",
    },
    "cori": {
        "piper": "en_GB-cori-high", "path": "en/en_GB/cori/high",
        "who": "a woman, British",
        "dataset": "LibriVox recordings, about 24 hours", "dataset_url": "https://librivox.org",
        "dataset_licence": "public domain", "training": "trained from scratch",
    },
    "ljspeech": {
        "piper": "en_US-ljspeech-high", "path": "en/en_US/ljspeech/high",
        "who": "a woman, American (also the story radio's Iona)",
        "dataset": "LJ Speech, LibriVox readings", "dataset_url": "https://keithito.com/LJ-Speech-Dataset/",
        "dataset_licence": "public domain", "training": "trained from scratch",
    },
}

# Delivery -> Chatterbox settings. Chatterbox's neutral exaggeration is 0.5 and
# cfg_weight 0.5; a lower cfg_weight slows the pace. `level_db` is added to the
# clip's voiced RMS (fall.finish levels it to -14 dB), and `slack` stretches the
# time limit for a slow delivery. What each is meant to give is a hope until the
# owner has heard it.
DELIVERIES = {
    # the sweet voice: high exaggeration, low cfg_weight
    "singsong": {"exaggeration": 0.85, "cfg_weight": 0.3, "temperature": 0.9, "level_db": 0.0, "slack": 1.8},
    # lips to the ear: the lowest exaggeration and cfg_weight, played quiet. A
    # hush, not a whisper: Chatterbox has no unvoiced speech.
    "whisper": {"exaggeration": 0.25, "cfg_weight": 0.2, "temperature": 0.7, "level_db": -9.0, "slack": 2.0},
    # the mask off: the highest exaggeration
    "snarl": {"exaggeration": 1.1, "cfg_weight": 0.4, "temperature": 0.85, "level_db": 0.0, "slack": 1.4},
    # no lift, no hurry: low exaggeration and temperature, full voice
    "menace": {"exaggeration": 0.3, "cfg_weight": 0.35, "temperature": 0.6, "level_db": 0.0, "slack": 1.6},
}

# Line number in HER_LINES_DRAFT.md -> the two deliveries to hear it in.
CANDIDATES = {
    2: ("singsong", "whisper"),
    16: ("snarl", "menace"),
    27: ("whisper", "menace"),
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


def clip_name(line_no, voice, delivery):
    return f"matron_{line_no:02d}_{voice}_{delivery}"


def seed_of(voice, line_no, delivery, take):
    return zlib.crc32(f"{voice}|{line_no}|{delivery}|{take}".encode())


def finish(samples, delivery):
    return fall.finish(samples, MIX_RATE, "") * float(10.0 ** (DELIVERIES[delivery]["level_db"] / 20.0))


def write_candidates(out, made):
    rows = [
        "# Her voice: candidates",
        "",
        "Written by src/voice/generate_matron_voice.py. Each clip is Chatterbox TTS (MIT licence) speaking one",
        "line from 03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md in the voice of a reference sentence spoken by",
        "a Piper voice (an AI voice, never a recording of a real person); Whisper checked that it says the line.",
        "Nothing else touches the voice (it is levelled and faded). Candidates only: nothing here is wired into",
        "the game.",
        "",
        "## Voices",
        "",
    ]
    for name, voice in VOICES.items():
        rows += [f"- **{name}**: {voice['who']}; Piper {voice['piper']} "
                 f"(https://huggingface.co/{PIPER_REPO}/tree/{PIPER_REVISION}/{voice['path']}), "
                 f"{voice['training']} on {voice['dataset']} ({voice['dataset_url']}), "
                 f"licence: {voice['dataset_licence']}"]
    rows += ["", "## Clips", "",
             "| clip | voice | line | delivery | exaggeration | cfg_weight | temperature | level dB | seed (take) | seconds | match | Whisper heard |",
             "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for item in made:
        d = DELIVERIES[item["delivery"]]
        rows.append(f"| {item['name']}.ogg | {item['voice']} | {item['line_no']} {item['line']!r} | {item['delivery']} "
                    f"| {d['exaggeration']} | {d['cfg_weight']} | {d['temperature']} | {-14.0 + d['level_db']:.0f} "
                    f"| {item['seed']} ({item['take']}) | {item['seconds']:.2f} | {item['match']:.2f} | {item['heard']!r} |")
    rows += ["", "Deliveries are what the settings are meant to give, not yet heard by anyone: `singsong` the sweet",
             "voice; `whisper` lips to the ear (a hush played quiet, not an unvoiced whisper: Chatterbox has none);",
             "`snarl` the mask off; `menace` no lift, no hurry.", ""]
    (out / "CANDIDATES.md").write_text("\n".join(rows))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(OUT))
    parser.add_argument("--models", required=True, help="the folder holding the Piper .onnx files")
    parser.add_argument("--work", default="voice_work", help="where the reference sentences are written")
    parser.add_argument("--takes", type=int, default=TAKES)
    parser.add_argument("--only", default="",
                        help="make only these clips again, by name, e.g. matron_02_kristin_singsong,matron_27_cori_menace")
    args = parser.parse_args()
    out = Path(args.out)
    only = {item for item in args.only.split(",") if item}
    text = draft_lines()
    missing = [n for n in CANDIDATES if n not in text]
    if missing:
        sys.exit(f"{LINES_DOC} has no line {missing}")

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
    references = {}
    for name, voice in VOICES.items():
        references[name] = work / f"reference_{name}.wav"
        subprocess.run([os.path.join(os.path.dirname(sys.executable), "piper"), "-m",
                        str(Path(args.models) / f"{voice['piper']}.onnx"), "-f", str(references[name])],
                       input=REFERENCE_TEXT.encode(), check=True)
    # The receipt is written after every clip, so a run cut short (a restarted
    # machine) is carried on by running it again: a clip already in the receipt
    # and on disk is kept. Delete the receipt to make every clip again.
    receipt = out / "generation_receipt.json"
    rows = {item["name"]: item for item in json.loads(receipt.read_text())} if receipt.exists() else {}
    rejected = []
    for line_no, deliveries in CANDIDATES.items():
        line = text[line_no]
        for voice in VOICES:
            for delivery in deliveries:
                name = clip_name(line_no, voice, delivery)
                if only and name not in only:
                    continue
                if not only and name in rows and (out / f"{name}.ogg").exists():
                    continue
                settings = DELIVERIES[delivery]
                limit = fall.time_limit(line) * settings["slack"]
                budget["tokens"] = int(25 * 1.5 * limit)
                best = None
                for take in range(args.takes):
                    seed = seed_of(voice, line_no, delivery, take)
                    torch.manual_seed(seed)
                    wav = tts.generate(line, audio_prompt_path=str(references[voice]),
                                       exaggeration=settings["exaggeration"], cfg_weight=settings["cfg_weight"],
                                       temperature=settings["temperature"])
                    raw = wav.squeeze(0).numpy()
                    clip = fall.trim(librosa.resample(raw, orig_sr=tts.sr, target_sr=MIX_RATE), MIX_RATE)
                    heard = asr.transcribe(librosa.resample(clip, orig_sr=MIX_RATE, target_sr=16000)
                                           .astype(np.float32), fp16=False, temperature=0.0)["text"].strip()
                    score = fall.match(line, heard)
                    seconds = len(clip) / MIX_RATE
                    said, wanted = len(fall.words(heard)), len(fall.words(line))
                    extra = said - wanted
                    # A take that says words of its own ("No. Waste not.") is as wrong as one that
                    # babbles: at most a quarter more words than the line has.
                    ok = score >= fall.MIN_MATCH and seconds <= limit and said <= wanted + wanted // 4
                    print(f"{name} take {take}: {seconds:.2f}s match {score:.2f} {'ok' if ok else 'REJECT'} {heard!r}",
                          flush=True)
                    if ok and (best is None or (score, -abs(extra), -seconds) >
                               (best["match"], -abs(best["extra"]), -best["seconds"])):
                        best = {"name": name, "voice": voice, "line_no": line_no, "line": line,
                                "delivery": delivery, "seed": seed, "take": take, "seconds": seconds,
                                "match": score, "extra": extra, "heard": heard, "clip": clip}
                    if best is not None and best["match"] >= 1.0 and best["extra"] == 0:
                        break
                if best is None:
                    rejected.append(f"{name} {line!r}")
                    continue
                sf.write(out / f"{name}.ogg", finish(best["clip"], delivery), MIX_RATE,
                         format="OGG", subtype="VORBIS")
                (out / f"{name}.ogg.import").write_text('[remap]\n\nimporter="keep"\n')
                rows[name] = {k: v for k, v in best.items() if k != "clip"}
                done = [rows[key] for key in sorted(rows)]
                receipt.write_text(json.dumps(done, indent=2) + "\n")
                write_candidates(out, done)

    if rejected:
        print("NO PASSING TAKE (run again with --only and more --takes):\n  " + "\n  ".join(rejected))
        sys.exit(1)


if __name__ == "__main__":
    main()
