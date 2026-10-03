from pathlib import Path
import re,json,math,hashlib
root=Path('/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao')
source=root/'repo/src/sim/world_solids.inc'
# CHOSEN screening volume, not a measured swept envelope.
lo=(-30.5,110.6,-159.5);hi=(-28.5,136.6,-156.5)
hits=[];count=0
for line in source.read_text().splitlines():
    if not line.lstrip().startswith('{') or '// ' not in line:continue
    nums=re.findall(r'-?\d+\.\d+F',line.split('//')[0])
    if len(nums)!=10:continue
    p=tuple(float(s[:-1]) for s in nums[:3]);qx,qy,qz,qw=(float(s[:-1]) for s in nums[3:7]);half=tuple(float(s[:-1]) for s in nums[7:])
    norm=math.sqrt(qx*qx+qy*qy+qz*qz+qw*qw);qx/=norm;qy/=norm;qz/=norm;qw/=norm
    mat=((1-2*(qy*qy+qz*qz),2*(qx*qy-qz*qw),2*(qx*qz+qy*qw)),(2*(qx*qy+qz*qw),1-2*(qx*qx+qz*qz),2*(qy*qz-qx*qw)),(2*(qx*qz-qy*qw),2*(qy*qz+qx*qw),1-2*(qx*qx+qy*qy)))
    extent=tuple(sum(abs(mat[i][j])*half[j] for j in range(3)) for i in range(3))
    lower=tuple(p[i]-extent[i] for i in range(3));upper=tuple(p[i]+extent[i] for i in range(3));count+=1
    if all(upper[i]>=lo[i] and lower[i]<=hi[i] for i in range(3)):
        hits.append({'name':line.split('// ',1)[1],'bounds':[lower,upper]})
text=source.read_text()
point_block=text.split('constexpr float kWorldSolidHullPoints[] = {',1)[1].split('};',1)[0]
coordinates=[float(v[:-1]) for v in re.findall(r'-?\d+\.\d+F',point_block)]
assert len(coordinates)%3==0
hull_block=text.split('constexpr WorldSolidHull kWorldSolidHulls[] = {',1)[1].split('};',1)[0]
hull_hits=[];hull_count=0
for line in hull_block.splitlines():
    if not line.lstrip().startswith('{'):continue
    match=re.fullmatch(r'\s*\{(\d+), (\d+)\},\s*// (.+)',line)
    assert match,line
    first,n,name=int(match[1]),int(match[2]),match[3]
    assert (first+n)*3<=len(coordinates)
    points=[coordinates[i:i+3] for i in range(first*3,(first+n)*3,3)]
    lower=tuple(min(p[i] for p in points) for i in range(3));upper=tuple(max(p[i] for p in points) for i in range(3));hull_count+=1
    if all(upper[i]>=lo[i] and lower[i]<=hi[i] for i in range(3)):
        hull_hits.append({'name':name,'bounds':[lower,upper]})
assert count==1316 and hull_count==666
report={'evidence':'Conservative AABB intersection screen of generated normal-world box/hull records. Not a native swept/collision/traversal proof.','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'chosen_ladder_plane':[-29.5,135.6,-158.0],'chosen_screen_bounds':[lo,hi],'boxes_checked':count,'box_intersections':hits,'hulls_checked':hull_count,'hull_intersections':hull_hits,'unresolved':['Authoritative native Kit assemblies must be checked.','Wind/catch/load/refinement must determine the true rope/rider envelope.','Entry, receiver, recovery and frame are not implemented.']}
(root/'design/rope-placement/box-screen.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
