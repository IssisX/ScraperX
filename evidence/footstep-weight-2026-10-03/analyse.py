from pathlib import Path
import hashlib,json,wave
import numpy as np

p=Path(__file__).resolve().parent

def read(path):
 with wave.open(str(path),'rb') as wav:
  assert wav.getnchannels()==1 and wav.getsampwidth()==2 and wav.getframerate()==22050
  return np.frombuffer(wav.readframes(wav.getnframes()),dtype='<i2').astype(float)/32768

def db(value): return float(20*np.log10(max(float(value),1e-12)))
def metrics(x):
 sr=22050; spec=abs(np.fft.rfft(x))**2; freq=np.fft.rfftfreq(len(x),1/sr); total=spec.sum()
 phone=spec[(freq>=300)&(freq<6000)]
 cumulative=np.cumsum(x*x); energy=cumulative[-1]
 def band(lo,hi): return float(spec[(freq>=lo)&(freq<hi)].sum()/total)
 return {'peak_dbfs':db(abs(x).max()),'rms_dbfs':db(np.sqrt(np.mean(x*x))),
  'centroid_hz':float((freq*spec).sum()/total),'energy_fraction_300_900':band(300,900),
  'energy_fraction_900_6000':band(900,6000),'energy_fraction_300_6000':band(300,6000),
  'phone_band_rms_dbfs':db(np.sqrt(2*phone.sum())/len(x)),
  'tail_after_70ms_energy_fraction':float(np.sum(x[int(.07*sr):]**2)/energy),
  'energy_95_percent_time_s':float(np.searchsorted(cumulative,.95*energy)/sr),
  'top_5_phone_bin_energy_fraction':float(np.sort(phone)[-5:].sum()/phone.sum()),
  'end_sample_absolute':float(abs(x[-1])),'duration_s':len(x)/sr}

names=sorted(f.name for f in (p/'before').glob('*.wav'))
assert len(names)==50 and names==sorted(f.name for f in (p/'after').glob('*.wav'))
result={'engine':'4.7.stable.official.5b4e0cb0f','exported_clips_per_bank':50,'unchanged_nonstep_clips':[], 'steps':{},'hashes':{}}
for name in names:
 for phase in ['before','after']:
  result['hashes'][phase+'/'+name]=hashlib.sha256((p/phase/name).read_bytes()).hexdigest()
 before=(p/'before'/name).read_bytes();after=(p/'after'/name).read_bytes()
 if name.startswith('step_'):
  assert before!=after,name
  result['steps'][name]={phase:metrics(read(p/phase/name)) for phase in ['before','after']}
  for phase in ['before','after']:
   x=read(p/phase/name)
   assert np.isfinite(x).all() and abs(x).max()<=.90001
   assert abs(x[-1])<.001
 else:
  assert before==after,'Unrelated seeded cue changed: '+name
  result['unchanged_nonstep_clips'].append(name)
assert len(result['unchanged_nonstep_clips'])==38
result['material_means']={}
for material in ['metal','concrete','earth']:
 result['material_means'][material]={phase:{metric:float(np.mean([v[phase][metric] for name,v in result['steps'].items() if name.startswith('step_'+material+'_')])) for metric in next(iter(result['steps'].values()))[phase]} for phase in ['before','after']}
 # Equal-RMS montages make timbre comparisons available without louder being better.
 for phase in ['before','after']:
  clips=[]
  for i in range(4):
   x=read(p/phase/f'step_{material}_{i}.wav');x*=.04/np.sqrt(np.mean(x*x))
   x=np.pad(x,(0,int(.5*22050)-len(x)));clips.append(x)
  x=np.concatenate(clips)
  assert abs(x).max()<1
  with wave.open(str(p/f'{phase}-{material}-matched-rms.wav'),'wb') as w:
   w.setnchannels(1);w.setsampwidth(2);w.setframerate(22050);w.writeframes((x*32767).astype('<i2').tobytes())
result['boundary']='PCM/export and numerical timbre evidence only. 300 Hz brickwall analysis is not a Fold 6 speaker response. No human listening, spatial mixer, live gait, APK or device result.'
(p/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS: 50 real Godot clips per bank; 12 steps changed; 38 other cues byte-identical; step peaks safe and ends silent.')
for material,x in result['material_means'].items():
 print(material, json.dumps({phase:{k:round(v,5) for k,v in values.items() if k in ['centroid_hz','energy_fraction_300_900','energy_fraction_900_6000','tail_after_70ms_energy_fraction','energy_95_percent_time_s','phone_band_rms_dbfs','top_5_phone_bin_energy_fraction']} for phase,values in x.items()}))
