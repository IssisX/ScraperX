"""Generates the climber's fear voice: the yells, swearing and relief that
GDD 8.4 / Governing Law 8 require on large falls.

Offline only. The game ships the Ogg Vorbis files this writes to
godot/presentation/audio/voice/ and the table voice_lines.gd; nothing here
runs on a device or in CI.

Source of every clip:
  * speech: Chatterbox TTS (Resemble AI, MIT licence), whose `exaggeration`
    drives the delivery from level to yelled;
  * voice identity: a reference sentence spoken by Piper's en_US-john-medium
    voice, itself trained on LibriVox public-domain recordings;
  * check: Whisper (OpenAI, MIT) transcribes each take; a take is rejected
    unless the transcript holds at least 60% of its line's distinct words and
    it ends within ~0.45 s a word (screams longer), so no clip ships that says
    something else, nothing at all, or babbles on. Every line ships: one that
    no take passes fails the run, to be made again with --only and --takes.

Setup (a throwaway venv, CPU only):
  python3 -m venv tts
  tts/bin/pip install torch==2.6.0 torchaudio==2.6.0 --index-url https://download.pytorch.org/whl/cpu
  tts/bin/pip install chatterbox-tts==0.1.7 piper-tts==1.8.0 openai-whisper soundfile
  curl -LO https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/john/medium/en_US-john-medium.onnx
  curl -LO https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/john/medium/en_US-john-medium.onnx.json
Run:
  tts/bin/python src/voice/generate_fall_voice.py --piper-model en_US-john-medium.onnx

Seeds are fixed per line and take, so a rerun on the same packages and CPU
reproduces the same selection.
"""

import argparse
import os
import re
import subprocess
import sys

import numpy as np

# Tier -> (exaggeration, [lines]). A line may carry its own exaggeration as
# (text, exaggeration). The director picks a tier from what the fall is
# doing; within a tier it draws without repeats.
#   yelp   -- the moment the ground goes: short, involuntary
#   panic  -- still falling after the yelp
#   terror -- a long fall: screaming, pleading, swearing
#   relief -- the canopy opens, a hand catches hold, or a long fall ends alive
#   pain   -- landed hard and lived
LINES = {
    "yelp": (1.3, [
        "Whoa, whoa, whoa!",
        "Oh shit, no!",
        "Oh fuck, no!",
        "Oh crap, oh crap!",
        "Nope, nope, nope!",
        "Oh no, oh no!",
        "Shit, shit, shit!",
        "Whoa, hey, hey!",
        "Ah, shit, no!",
        "Fuck, fuck!",
    ]),
    "panic": (1.6, [
        "Oh shit, oh shit, oh shit!",
        "No no no no no no!",
        "Fuck, fuck, fuck, fuck!",
        "Oh fuck, oh fuck, oh fuck!",
        "Shit shit shit shit shit!",
        "Nope nope nope nope nope!",
        "Oh god, oh god, oh god!",
        "This was a terrible idea!",
        "Oh, come on! Come on!",
        "I should have taken the stairs!",
        "Grab something, grab something!",
        "Why? Why? Why?",
        "Oh, motherfucker!",
    ]),
    "terror": (1.9, [
        ("Aaaah! Aaaaaaaah!", 1.2),  # a pure scream babbles at 1.9
        "Aaaah! Fuck! Fuck!",
        "I'm gonna die, I'm gonna die, I'm gonna die!",
        "Mother fucker!",
        "Not like this! Not like this!",
        "Somebody catch me!",
        "Holy shit, holy shit, holy shit!",
        "Why did I climb this fucking thing?",
        "Oh god, I'm sorry, Mom!",
        "Aaaah! Shit! Shit!",
        "Help! Help me! Help!",
        "No! No! Aaaaah!",
    ]),
    "relief": (0.8, [
        "Oh, thank god. Thank god.",
        "Ha. Ha ha. Okay. I'm alive.",
        "Holy shit. Holy shit.",
        "Never doing that again.",
        "Okay. Okay. Breathe.",
        "Oh, thank fuck for that.",
    ]),
    "pain": (1.1, [
        "Ow! Fuck!",
        "Oof. Ow, shit.",
        "Ah, my ankles!",
        "Ugh. That was stupid.",
        "Ow, ow, ow, ow.",
        "Argh, son of a bitch!",
    ]),
}

REFERENCE_TEXT = "Whoa! Hey, watch out! I can't hold on, this is way too high up here!"
TAKES = 3
MIX_RATE = 22050
MIN_MATCH = 0.6


def words(text):
    return re.findall(r"[a-z']+", text.lower().replace("*", ""))


def match(expected, heard):
    """Fraction of the line's distinct words the transcript contains. A word
    also counts when the transcript runs it into its neighbours, as it writes
    "Mother fucker" as "MOTHERFUCKER" (a first version compared whole words
    only and threw that take away). A scream ("Aaaah") transcribes
    unpredictably, so vowel-only words count as heard when the transcript
    holds any drawn-out vowel."""
    want = set(words(expected))
    got = set(words(heard))
    run = re.sub(r"[^a-z]", "", heard.lower())
    if not want:
        return 0.0
    hit = 0
    for w in want:
        if (w in got or w.replace("'", "") in run
                or (re.fullmatch(r"a+h*", w) and re.search(r"a{2,}|ah", heard.lower()))):
            hit += 1
    return hit / len(want)


def time_limit(line):
    """How long a take may run before it has babbled: ~0.45 s a word, and a
    scream's drawn-out vowel (Aaaah) 0.6 s more."""
    spoken = words(line)
    screams = sum(1 for w in spoken if re.fullmatch(r"a+h*", w))
    return 1.0 + 0.45 * len(spoken) + 0.6 * screams


def trim(samples, rate, floor_db=-40.0):
    """Cuts leading and trailing silence: a yell must start the instant the
    fall does."""
    frame = int(0.01 * rate)
    level = np.array([np.sqrt(np.mean(samples[i:i + frame] ** 2) + 1e-12)
                      for i in range(0, len(samples) - frame, frame)])
    loud = np.nonzero(20.0 * np.log10(level / (level.max() + 1e-12)) > floor_db)[0]
    if loud.size == 0:
        return samples
    first = max(0, loud[0] * frame - frame)
    last = min(len(samples), (loud[-1] + 2) * frame)
    return samples[first:last]


def finish(samples, rate, tier):
    """Shouted tiers get a little saturation (a throat at its limit), then
    every clip is levelled on its voiced part and faded at both ends."""
    if tier in ("yelp", "panic", "terror"):
        drive = 1.8
        samples = np.tanh(samples / (np.abs(samples).max() + 1e-9) * drive) / np.tanh(drive)
    frame = int(0.02 * rate)
    rms = np.array([np.sqrt(np.mean(samples[i:i + frame] ** 2))
                    for i in range(0, max(1, len(samples) - frame), frame)])
    voiced = rms[rms > 0.25 * rms.max()]
    target = 10.0 ** (-14.0 / 20.0)
    samples = samples * (target / (np.sqrt(np.mean(voiced ** 2)) + 1e-9))
    peak = np.abs(samples).max()
    ceiling = 10.0 ** (-1.0 / 20.0)
    if peak > ceiling:
        samples = samples * (ceiling / peak)
    fade_in = int(0.005 * rate)
    fade_out = int(0.03 * rate)
    samples[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    samples[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)
    return samples.astype(np.float32)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--piper-model", required=True)
    parser.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "..",
                                                      "godot", "presentation", "audio", "voice"))
    parser.add_argument("--work", default="voice_work")
    parser.add_argument("--only", default="",
                        help="regenerate only these lines, e.g. terror:3,yelp:5 (tier:index)")
    parser.add_argument("--takes", type=int, default=TAKES)
    args = parser.parse_args()
    only = {tuple(item.split(":")) for item in args.only.split(",") if item}

    import librosa
    import soundfile as sf
    import torch
    import whisper
    from chatterbox.tts import ChatterboxTTS

    os.makedirs(args.work, exist_ok=True)
    os.makedirs(args.out, exist_ok=True)
    reference = os.path.join(args.work, "reference.wav")
    subprocess.run([os.path.join(os.path.dirname(sys.executable), "piper"), "-m", args.piper_model,
                    "-f", reference], input=REFERENCE_TEXT.encode(), check=True)

    tts = ChatterboxTTS.from_pretrained(device="cpu")
    # generate() asks for up to 1000 speech tokens (40 s at 25 tokens/s); a
    # one-word line at high exaggeration can babble to that cap. Each take
    # gets 1.5x its accepted length instead.
    inference = tts.t3.inference
    budget = {"tokens": 1000}

    def capped(*a, **k):
        k["max_new_tokens"] = budget["tokens"]
        return inference(*a, **k)

    tts.t3.inference = capped
    asr = whisper.load_model("small.en")
    report = []
    rejected = []
    for tier, (exaggeration, lines) in LINES.items():
        for index, entry in enumerate(lines):
            if only and (tier, str(index)) not in only:
                continue
            line, strength = (entry, exaggeration) if isinstance(entry, str) else entry
            best = None
            limit = time_limit(line)
            budget["tokens"] = int(25 * 1.5 * limit)
            for take in range(args.takes):
                torch.manual_seed(1000 * index + 17 * take + len(tier))
                wav = tts.generate(line, audio_prompt_path=reference, exaggeration=strength,
                                   cfg_weight=0.3, temperature=0.9)
                raw = wav.squeeze(0).numpy()
                clip = trim(librosa.resample(raw, orig_sr=tts.sr, target_sr=MIX_RATE), MIX_RATE)
                heard = asr.transcribe(librosa.resample(clip, orig_sr=MIX_RATE, target_sr=16000)
                                       .astype(np.float32), fp16=False, temperature=0.0)["text"]
                score = match(line, heard)
                seconds = len(clip) / MIX_RATE
                # A take that runs on far past its words has babbled.
                ok = score >= MIN_MATCH and seconds <= limit
                print(f"{tier}/{index} take {take}: {seconds:.2f}s match {score:.2f} {heard!r}",
                      flush=True)
                if ok and (best is None or score > best[0] or
                           (score == best[0] and seconds < best[1])):
                    best = (score, seconds, clip, heard)
                if best is not None and best[0] >= 1.0:
                    break
            if best is None:
                rejected.append(f"{tier}:{index} {line!r}")
                continue
            # Named for the line's place in LINES, so one line can be made
            # again (--only) without renumbering the rest.
            name = f"{tier}_{index:02d}.ogg"
            sf.write(os.path.join(args.out, name), finish(best[2], MIX_RATE, tier), MIX_RATE,
                     format="OGG", subtype="VORBIS")
            with open(os.path.join(args.out, name + ".import"), "w") as marker:
                marker.write('[remap]\n\nimporter="keep"\n')
            report.append(f"{name} {best[1]:.2f}s match {best[0]:.2f} line {line!r} heard {best[3]!r}")

    # The table lists every clip on disk, so a partial run (--only) keeps
    # the lines it did not touch.
    present = set(os.listdir(args.out))
    table = {tier: [f"{tier}_{index:02d}.ogg" for index in range(len(lines))
                    if f"{tier}_{index:02d}.ogg" in present]
             for tier, (_, lines) in LINES.items()}
    with open(os.path.join(args.out, "..", "voice_lines.gd"), "w") as gd:
        gd.write("extends RefCounted\n")
        gd.write("# Written by src/voice/generate_fall_voice.py -- do not edit by hand.\n")
        gd.write("# Tier -> clip files in presentation/audio/voice/.\n\n")
        gd.write("const DIR := \"res://presentation/audio/voice/\"\n")
        gd.write("const TIERS := {\n")
        for tier, names in table.items():
            gd.write(f"\t&\"{tier}\": [\n")
            for name in names:
                gd.write(f"\t\t\"{name}\",\n")
            gd.write("\t],\n")
        gd.write("}\n")
    print("\n".join(report))
    # Every scripted line ships. One that no take could pass stops the run
    # loudly, for more takes or a rewording -- it is never left out quietly.
    if rejected:
        print("NO PASSING TAKE (run again with --only and more --takes):\n  " + "\n  ".join(rejected))
        sys.exit(1)


if __name__ == "__main__":
    main()
