# Builds the review sheets from the probe renders.
import os, subprocess, sys
T = '/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/overhaul/skin_face_eyes'
R = T + '/renders'; S = T + '/tools/strip.py'; OUT = T + '/sheets'; os.makedirs(OUT, exist_ok=True)
def strip(out, crop, items):
    subprocess.run([sys.executable, S, out, str(crop)] + [f'{a}={b}' for a, b in items], check=True)
from PIL import Image
def stack(out, files):
    ims = [Image.open(f) for f in files]; W = max(i.size[0] for i in ims); H = sum(i.size[1] for i in ims)
    S_ = Image.new('RGB', (W, H)); y = 0
    for i in ims: S_.paste(i, (0, y)); y += i.size[1]
    S_.save(out, quality=88) if out.endswith('.jpg') else S_.save(out)
yaws = ['-070', '-035', '+000', '+035', '+070']
for var, d in (('real', 'tt_real'), ('wrong', 'tt_wrong')):
    if not os.path.isdir(f'{R}/{d}'): continue
    parts = []
    for rig in ('day', 'night'):
        for dist, crop, tag in (('045', 600, '0.45 m'), ('130', 300, '1.3 m')):
            out = f'{R}/_{var}_{rig}_{dist}.png'
            strip(out, crop, [(f'{var} {rig} {tag} yaw {y}', f'{R}/{d}/{var}_{rig}_d{dist}_y{y}.png') for y in yaws])
            parts.append(out)
    stack(f'{OUT}/turntable_{var}_045.jpg', [parts[0], parts[2]])
    stack(f'{OUT}/turntable_{var}_130.jpg', [parts[1], parts[3]])
print('ok')
