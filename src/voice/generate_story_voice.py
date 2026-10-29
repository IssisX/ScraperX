"""Generates the story radio's spoken dialogue: every cue in
godot/audio/story_radio/manifest.json, in its speaker's voice, made the way
the climber's fear yells are (src/voice/generate_fall_voice.py).

Offline only. The game ships the Ogg Vorbis files this writes to
godot/audio/story_radio/ with manifest.json; nothing here runs on a device or
in CI.

Source of every clip:
  * speech: Chatterbox TTS (Resemble AI, MIT licence), its `exaggeration`
    held near its neutral 0.5 so the radio talk is calm and spoken, raised a
    little only where a line is heated (LINE_DELIVERY);
  * voice identity: one reference passage per speaker, spoken by a Piper voice
    trained on public-domain or permissively licensed recordings (VOICES);
    Ro's is the climber's own Piper en_US-john, and Ro also voices the handover
    terminal's RECORD and SOURCE playback;
  * check: Whisper (OpenAI, MIT) transcribes each take; a take is rejected
    unless the transcript holds at least 60% of its line's distinct words
    (generate_fall_voice.match, after spelling numbers and units out), it ends
    within ~0.45 s a word, and it does not say a quarter more words than the
    line has, so no clip ships that says something else, nothing at all, or
    babbles on. The best passing take ships. Every line ships: one that no
    take passes fails the run, to be made again with --only and --takes.

Each kept take is trimmed, padded (0.15 s before, 0.35 s after), written clean
to source/<cue>.wav (not imported by Godot), then put through the same chain
the radio has always used: ffmpeg highpass 180 Hz, lowpass 4800 Hz, loudnorm
-18 LUFS / -2 dBTP for the player's radio; highpass 70 Hz and the same
loudnorm for the voices heard in the room (Ari, the terminal's RECORD and
SOURCE). manifest.json gets each cue's duration, hash and 30 Hz speech
envelope (the mouth and radio meter read it); generation_receipt.json gets
the seed, every take's transcript and the hashes.

Setup (a throwaway venv, CPU only; the same one generate_fall_voice.py uses):
  python3.12 -m venv tts
  tts/bin/pip install torch==2.6.0 torchaudio==2.6.0 --index-url https://download.pytorch.org/whl/cpu
  tts/bin/pip install chatterbox-tts==0.1.7 piper-tts==1.8.0 openai-whisper soundfile
  mkdir models && cd models
  B=https://huggingface.co/rhasspy/piper-voices/resolve/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd
  for v in en/en_US/ljspeech/high/en_US-ljspeech-high en/en_US/john/medium/en_US-john-medium \\
           en/en_US/bryce/medium/en_US-bryce-medium; do
    curl -LO $B/$v.onnx -LO $B/$v.onnx.json; done
Run (from the repository root; ffmpeg and ffprobe on PATH):
  tts/bin/python src/voice/generate_story_voice.py --models models
Remake some lines only (the rest stay as they are):
  tts/bin/python src/voice/generate_story_voice.py --models models --only double_ro_who,double_ari_name --takes 6
Rerun only the radio chain from source/*.wav (no generation, no models):
  python3 src/voice/generate_story_voice.py --reprocess

Seeds are fixed per cue and take, so a rerun on the same packages and CPU
reproduces the same selection.
"""

import argparse
from datetime import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib
from zoneinfo import ZoneInfo

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# The fall voice's line matcher and silence trim, imported as they are: that
# generator and its clips are finished and stay untouched.
import generate_fall_voice as fall  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
RADIO = ROOT / "godot" / "audio" / "story_radio"
NOTICE = "VOICE_LICENSES.txt"

CHATTERBOX = "ResembleAI/chatterbox"
CHATTERBOX_REVISION = "5bb1f6ee58e50c3b8d408bc82a6d3740c2db6e18"
PIPER_REPO = "rhasspy/piper-voices"
PIPER_REVISION = "5041a7a49ae5b3a0f30e6837769eb03fdbb231bd"
WHISPER = "small.en"
VERSIONS = {"torch": "2.6.0 (CPU)", "chatterbox-tts": "0.1.7", "piper-tts": "1.8.0",
            "openai-whisper": "20250625", "ffmpeg": "6.1.1"}

# Spoken by every speaker's Piper voice to give Chatterbox the voice: calm,
# level radio talk, none of it a line of the story.
REFERENCE_TEXT = ("Copy that. The night crew has finished on the north side, and the lift is "
                  "locked at the second ring. Check in when you reach the landing.")

# Speaker -> the Piper voice that gives it its identity. Each model card's
# dataset and licence, read at PIPER_REVISION, are written down with it.
VOICES = {
    "IONA": {
        "piper": "en_US-ljspeech-high", "path": "en/en_US/ljspeech/high", "speaker": None,
        "language": "American English", "who": "a woman, American",
        "dataset": "LJ Speech (Keith Ito and Linda Johnson, 2017), LibriVox readings",
        "dataset_url": "https://keithito.com/LJ-Speech-Dataset/",
        "dataset_license": "Public domain",
        "training": "Trained from scratch on LJ Speech by Bryce Beattie; model released public domain",
    },
    "RO": {
        "piper": "en_US-john-medium", "path": "en/en_US/john/medium", "speaker": None,
        "language": "American English", "who": "a man, American (the climber's fall voice)",
        "dataset": "LibriVox recordings, about 12.5 hours (Bryce Beattie)",
        "dataset_url": "https://librivox.org",
        "dataset_license": "Public domain",
        "training": "Fine-tuned from Piper en_US-kristin-medium (LibriVox, public domain, trained from scratch)",
    },
    "ARI": {
        "piper": "en_US-bryce-medium", "path": "en/en_US/bryce/medium", "speaker": None,
        "language": "American English", "who": "a man, American",
        "dataset": "Bryce Beattie's own recorded voice, about 750 samples",
        "dataset_url": "https://brycebeattie.com/files/tts/",
        "dataset_license": "Public domain",
        "training": "Fine-tuned by Bryce Beattie from an unreleased voice of his; model released public domain",
    },
    # DEV (a man, British) has no voice yet: every British man's Piper voice
    # is fine-tuned from en_US-lessac, whose Blizzard 2013 data is not
    # cleared for a commercial game. A Dev cue fails until one is chosen.
}
VOICE_OF = {"RECORD": "RO", "SOURCE": "RO"}

# Delivery: Chatterbox's neutral exaggeration is 0.5. Iona, Ro and Dev talk at
# it; Ari is quieter; the terminal plays back a flat, formal recording.
DELIVERY = {"IONA": 0.5, "RO": 0.5, "ARI": 0.45, "RECORD": 0.4, "SOURCE": 0.4, "DEV": 0.5}
# Lines whose text is heated get a little more; nothing here is shouted.
LINE_DELIVERY = {
    "double_ro_who": 0.65,
    "double_ro_face": 0.6,
    "double_ro_empty": 0.6,
    "double_ro_name": 0.6,
    "vale_contact_iona": 0.6,
    "handover_ro_denial": 0.55,
}
CFG_WEIGHT = 0.5
TEMPERATURE = 0.8
TAKES = 3
MIN_MATCH = fall.MIN_MATCH
RATE = 24000
LEAD_S = 0.15
TAIL_S = 0.35

DIRECT = {"ARI", "RECORD", "SOURCE"}
FILTER = "highpass=f=180,lowpass=f=4800,loudnorm=I=-18:TP=-2:LRA=9"
DIRECT_FILTER = "highpass=f=70,loudnorm=I=-18:TP=-2:LRA=9"
PLACEMENT = {"ARI": "native actor head / spatial", "RECORD": "handover terminal speaker / spatial",
             "SOURCE": "handover terminal speaker / spatial"}

ONES = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
        "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen",
        "nineteen"]
TENS = ["", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"]
SPELLING = {"meters": "metres", "meter": "metre", "m": "metres"}


def spoken_number(n):
    if n < 20:
        return ONES[n]
    if n < 100:
        return TENS[n // 10] + ("" if n % 10 == 0 else " " + ONES[n % 10])
    if n < 1000:
        rest = n % 100
        return ONES[n // 100] + " hundred" + ("" if rest == 0 else " and " + spoken_number(rest))
    rest = n % 1000
    return spoken_number(n // 1000) + " thousand" + ("" if rest == 0 else " " + spoken_number(rest))


def normalize(text):
    """The words a line says, in one spelling: curly apostrophes straight,
    hyphens and digits spelled out ("253" -> "two hundred and fifty three"),
    and Whisper's American "meters" as the script's "metres"."""
    text = text.replace("’", "'").replace("‘", "'").replace("-", " ")
    text = re.sub(r"(\d+)(m)\b", r"\1 \2", text)
    text = re.sub(r"\d+", lambda m: " " + spoken_number(int(m.group())) + " ", text.replace(",", ""))
    return " ".join(SPELLING.get(w, w) for w in fall.words(text))


def score(line, heard):
    """(match, words heard, words in the line): match is the fall voice's
    fraction of the line's distinct words the transcript holds."""
    want, got = normalize(line), normalize(heard)
    return fall.match(want, got), len(got.split()), len(want.split())


def time_limit(line):
    return 1.0 + 0.45 * len(normalize(line).split())


def seed_for(cue, take):
    return zlib.crc32(cue.encode()) % 100000 * 10 + take


def central_now():
    return datetime.now(ZoneInfo("America/Chicago")).isoformat(timespec="seconds")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def voice_of(speaker):
    return VOICES[VOICE_OF.get(speaker, speaker)]


def piper_label(voice):
    return voice["piper"] + (f" speaker {voice['speaker']}" if voice["speaker"] else "")


def process(cue, speaker):
    """The radio chain, unchanged from the Kokoro library's: clean WAV in,
    shipped Ogg out; returns (duration_s, sha256, rms_30hz)."""
    clean = RADIO / "source" / f"{cue}.wav"
    target = RADIO / f"{cue}.ogg"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(clean),
                    "-af", DIRECT_FILTER if speaker in DIRECT else FILTER, "-ar", str(RATE), "-ac", "1",
                    "-c:a", "libvorbis", "-q:a", "5", str(target)], check=True)
    duration = float(subprocess.check_output(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "default=noprint_wrappers=1:nokey=1", str(target)], text=True))
    # The shipped recording's own level, 30 times a second: mouth motion and
    # the radio meter sample it by playback position.
    pcm = subprocess.check_output(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(target),
                                   "-f", "f32le", "-ar", str(RATE), "-ac", "1", "-"])
    samples = [item[0] for item in struct.iter_unpack("<f", pcm)]
    envelope = []
    for start in range(0, len(samples), 800):
        window = samples[start:start + 800]
        rms = math.sqrt(sum(value * value for value in window) / len(window))
        envelope.append(round(min(1.0, rms * 5.0), 6))
    return duration, sha256(target), envelope


def voice_entry(speaker, generated):
    voice = voice_of(speaker)
    generation = {
        "method": "src/voice/generate_story_voice.py",
        "model": f"{CHATTERBOX} (Chatterbox TTS, English)",
        "model_revision": CHATTERBOX_REVISION,
        "reference_voice": f"{PIPER_REPO} {voice['piper']}",
        "reference_speaker": voice["speaker"],
        "reference_text": REFERENCE_TEXT,
        "exaggeration": DELIVERY[speaker],
        "cfg_weight": CFG_WEIGHT,
        "temperature": TEMPERATURE,
        "device": "CPU",
        "language": voice["language"],
        "check": f"openai-whisper {WHISPER}: at least {MIN_MATCH:.0%} of the line's distinct words, "
                 "within 1.0 s + 0.45 s a word, at most a quarter more words than the line",
        "radio_filter": DIRECT_FILTER if speaker in DIRECT else FILTER,
        "sample_rate_hz": RATE,
        "placement": PLACEMENT.get(speaker, "player radio"),
        "tools": VERSIONS,
    }
    if speaker in VOICE_OF:
        generation["voiced_by"] = VOICE_OF[speaker]
    return {
        "generation": generation,
        "provenance": {
            "piper_voices_revision": PIPER_REVISION,
            "reference_dataset": voice["dataset"],
            "reference_training": voice["training"],
            "generated": generated,
            "clean_sources": "source/*.wav (excluded from Godot import)",
            "receipt": "generation_receipt.json",
            "listening_review": "Whisper transcript of every take in generation_receipt.json; "
                                "listened to by the owner, not by the generator",
        },
        "license": {
            "model": "MIT (Chatterbox TTS, Resemble AI)",
            "reference_voice": f"Piper voice model MIT ({PIPER_REPO}); "
                               f"its training data: {voice['dataset_license']}",
            "notice": NOTICE,
            "commercial_use": "Chatterbox TTS is MIT licensed. The voice comes from a reference passage "
                              f"spoken by Piper {piper_label(voice)}, trained on {voice['dataset']} "
                              f"({voice['dataset_license']}). Dialogue is original project text. "
                              "Chatterbox marks its output with Resemble AI's inaudible Perth watermark. "
                              "No exclusive rights or clearance beyond the published terms are asserted.",
        },
        "source": [
            f"https://huggingface.co/{CHATTERBOX}/tree/{CHATTERBOX_REVISION}",
            "https://github.com/resemble-ai/chatterbox",
            f"https://huggingface.co/{PIPER_REPO}/blob/{PIPER_REVISION}/{voice['path']}/MODEL_CARD",
            voice["dataset_url"],
            "https://github.com/openai/whisper",
        ],
    }


def remade(receipt, cue):
    return receipt.get(cue, {}).get("event_id", "").startswith("chatterbox:")


def update_voices(manifest, receipt):
    """A speaker whose cues are all remade carries this method's provenance as
    its own. One whose other cues are still the old Kokoro takes keeps that
    entry (and its notice) for them, with this method's provenance under
    "remade", naming the cues it covers."""
    cues = manifest["cues"]
    for speaker in sorted({cue["speaker"] for cue in cues.values()}):
        own = [key for key, cue in cues.items() if cue["speaker"] == speaker]
        new = [key for key in own if remade(receipt, key)]
        if not new:
            continue
        entry = voice_entry(speaker, max(receipt[key]["generated_central"][:10] for key in new))
        if len(new) == len(own):
            manifest["voices"][speaker] = entry
        else:
            kept = {k: v for k, v in manifest["voices"][speaker].items() if k != "remade"}
            manifest["voices"][speaker] = dict(kept, remade=dict(entry, cues=new))


def save(manifest, receipt):
    update_voices(manifest, receipt)
    (RADIO / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    (RADIO / "generation_receipt.json").write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--models", help="directory holding the Piper .onnx voices")
    parser.add_argument("--work", default="story_voice_work")
    parser.add_argument("--only", default="", help="comma-separated cue ids to remake")
    parser.add_argument("--takes", type=int, default=TAKES)
    parser.add_argument("--reprocess", action="store_true",
                        help="rerun only the radio chain on source/*.wav; generate nothing")
    args = parser.parse_args()

    manifest = json.loads((RADIO / "manifest.json").read_text())
    receipt_path = RADIO / "generation_receipt.json"
    receipt = json.loads(receipt_path.read_text()) if receipt_path.exists() else {}
    cues = manifest["cues"]
    only = [item for item in args.only.split(",") if item]
    unknown = [item for item in only if item not in cues]
    if unknown:
        parser.error("not cues in manifest.json: " + ", ".join(unknown))
    chosen = [cue for cue in cues if not only or cue in only]
    voiceless = sorted({cues[cue]["speaker"] for cue in chosen} - set(VOICES) - set(VOICE_OF))
    if voiceless and not args.reprocess:
        parser.error("no cleared reference voice yet for " + ", ".join(voiceless) + " (VOICES)")

    if args.reprocess:
        for cue in chosen:
            speaker = cues[cue]["speaker"]
            duration, digest, envelope = process(cue, speaker)
            cues[cue].update({"duration_s": duration, "rms_30hz": envelope, "rms_audio_sha256": digest})
            receipt[cue]["radio_sha256"] = digest
            print(f"{cue} reprocessed {duration:.2f}s", flush=True)
        save(manifest, receipt)
        return
    if not args.models:
        parser.error("--models is required to generate")

    import librosa
    import numpy as np
    import soundfile as sf
    import torch
    import whisper
    from chatterbox.tts import ChatterboxTTS

    torch.set_num_threads(os.cpu_count() or 4)
    work = Path(args.work)
    work.mkdir(parents=True, exist_ok=True)
    (RADIO / "source").mkdir(exist_ok=True)
    (RADIO / "source" / ".gdignore").write_text("")

    references = {}
    for name in sorted({VOICE_OF.get(cues[cue]["speaker"], cues[cue]["speaker"]) for cue in chosen}):
        voice = VOICES[name]
        reference = work / f"reference_{name.lower()}.wav"
        command = [os.path.join(os.path.dirname(sys.executable), "piper"),
                   "-m", str(Path(args.models) / f"{voice['piper']}.onnx"), "-f", str(reference)]
        if voice["speaker"]:
            speakers = json.loads(Path(args.models, f"{voice['piper']}.onnx.json").read_text())
            command += ["-s", str(speakers["speaker_id_map"][voice["speaker"]])]
        subprocess.run(command, input=REFERENCE_TEXT.encode(), check=True)
        references[name] = str(reference)

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
    report, rejected, failed = [], [], []
    for cue in chosen:
        entry = cues[cue]
        line, speaker = entry["text"], entry["speaker"]
        voice_name = VOICE_OF.get(speaker, speaker)
        strength = LINE_DELIVERY.get(cue, DELIVERY[speaker])
        limit = time_limit(line)
        budget["tokens"] = int(25 * 1.5 * limit)
        best, takes = None, []
        for take in range(args.takes):
            seed = seed_for(cue, take)
            torch.manual_seed(seed)
            wav = tts.generate(line, audio_prompt_path=references[voice_name], exaggeration=strength,
                               cfg_weight=CFG_WEIGHT, temperature=TEMPERATURE)
            clip = fall.trim(wav.squeeze(0).numpy().astype(np.float32), tts.sr)
            if tts.sr != RATE:
                clip = librosa.resample(clip, orig_sr=tts.sr, target_sr=RATE)
            heard = asr.transcribe(librosa.resample(clip, orig_sr=RATE, target_sr=16000).astype(np.float32),
                                   fp16=False, temperature=0.0)["text"].strip()
            match, said, wanted = score(line, heard)
            seconds = len(clip) / RATE
            reasons = []
            if match < MIN_MATCH:
                reasons.append(f"match {match:.2f} < {MIN_MATCH}")
            if seconds > limit:
                reasons.append(f"{seconds:.2f}s > {limit:.2f}s")
            if said > wanted + max(2, wanted // 4):
                reasons.append(f"{said} words for {wanted}")
            takes.append({"take": take, "seed": seed, "seconds": round(seconds, 3),
                          "match": round(match, 3), "heard": heard,
                          "rejected": "; ".join(reasons) or None})
            print(f"{cue} take {take} seed {seed}: {seconds:.2f}s match {match:.2f} "
                  f"{'REJECT ' + '; '.join(reasons) if reasons else 'ok'} {heard!r}", flush=True)
            if reasons:
                rejected.append(f"{cue}:{take}")
            else:
                closeness = abs(said - wanted)
                rank = (round(match, 3), -closeness, -seconds)
                if best is None or rank > best[0]:
                    best = (rank, take, seed, clip, heard, match, seconds)
                if match >= 1.0 and closeness == 0:
                    break
        if best is None:
            failed.append(f"{cue} {line!r}")
            continue
        _, take, seed, clip, heard, match, seconds = best
        fade_in, fade_out = int(0.005 * RATE), int(0.03 * RATE)
        clip = clip.copy()
        clip[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
        clip[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)
        padded = np.concatenate([np.zeros(int(LEAD_S * RATE), np.float32), clip,
                                 np.zeros(int(TAIL_S * RATE), np.float32)])
        peak = np.abs(padded).max()
        if peak > 0.98:
            padded *= 0.98 / peak
        clean = RADIO / "source" / f"{cue}.wav"
        sf.write(clean, padded, RATE, subtype="PCM_16")
        duration, digest, envelope = process(cue, speaker)
        entry.pop("generated_utc", None)
        entry.update({"duration_s": duration, "rms_30hz": envelope, "rms_audio_sha256": digest})
        generated = central_now()
        receipt[cue] = {
            "event_id": f"chatterbox:{cue}:seed{seed}",
            "text": line,
            "speaker": speaker,
            "voice": f"Chatterbox from Piper {piper_label(VOICES[voice_name])}",
            "exaggeration": strength,
            "cfg_weight": CFG_WEIGHT,
            "temperature": TEMPERATURE,
            "seed": seed,
            "take": take,
            "whisper": heard,
            "match": round(match, 3),
            "takes": takes,
            "generated_central": generated,
            "clean_sha256": sha256(clean),
            "radio_sha256": digest,
        }
        save(manifest, receipt)
        report.append(f"{cue} {duration:.2f}s match {match:.2f} take {take} heard {heard!r}")

    # No receipt names a cue the manifest lacks.
    for stale in set(receipt) - set(cues):
        del receipt[stale]
    save(manifest, receipt)
    print("\n".join(report))
    print(f"REJECTED TAKES {len(rejected)}: {', '.join(rejected) or 'none'}")
    # Every line ships. One that no take could pass stops the run loudly, for
    # more takes; it is never left out quietly.
    if failed:
        print("NO PASSING TAKE (run again with --only and more --takes):\n  " + "\n  ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
