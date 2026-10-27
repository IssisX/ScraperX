import sys, glob, os, wave
import numpy as np
d = sys.argv[1]
rows = []
for p in sorted(glob.glob(os.path.join(d, "*.wav"))):
    w = wave.open(p)
    n = w.getnframes(); sr = w.getframerate()
    x = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float64) / 32768.0
    if x.size == 0: continue
    X = np.fft.rfft(x * np.hanning(x.size))
    f = np.fft.rfftfreq(x.size, 1.0 / sr)
    P = np.abs(X) ** 2
    tot = P.sum()
    band = P[(f >= 300) & (f <= 6000)].sum() / tot
    low = P[f < 300].sum() / tot
    cent = (P * f).sum() / tot
    # in-band RMS (approx, via FFT mask on unwindowed signal)
    Xr = np.fft.rfft(x); Xr[(f < 300) | (f > 6000)] = 0
    xb = np.fft.irfft(Xr, x.size)
    rms_db = 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)
    rmsb_db = 20 * np.log10(np.sqrt(np.mean(xb ** 2)) + 1e-12)
    # loudest 100 ms window in band
    win = int(0.1 * sr)
    if x.size > win:
        e = np.convolve(xb ** 2, np.ones(win) / win, mode="valid")
        loud_db = 10 * np.log10(e.max() + 1e-12)
    else:
        loud_db = rmsb_db
    peak_db = 20 * np.log10(np.abs(x).max() + 1e-12)
    seam = abs(x[0] - x[-1])
    name = os.path.basename(p)[:-4]
    rows.append((name, x.size / sr, band, low, cent, rms_db, rmsb_db, loud_db, peak_db, seam))
print("%-34s %5s %5s %5s %6s %6s %6s %6s %6s %6s" % ("clip", "sec", "band", "low", "cent", "rms", "rmsB", "loudB", "peak", "seam"))
for r in rows:
    print("%-34s %5.2f %5.2f %5.2f %6.0f %6.1f %6.1f %6.1f %6.1f %6.3f" % r)
