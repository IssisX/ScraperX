#!/usr/bin/env python3
"""Design proposal only. Analytic geometry checks; not a native walking proof.
Run: python check_bridge_geometry.py [local_repo]
Reads the production exported solids and native constants. Writes sibling SVG/JSON.
"""
import sys, re, math, json, pathlib, hashlib, subprocess
ROOT=pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else pathlib.Path(__file__).resolve().parents[4]
OUT=pathlib.Path(__file__).resolve().parent
CPP=(ROOT/'src/sim/simulation.cpp').read_text()
SOLID=(ROOT/'src/sim/world_solids.inc').read_text()
def const(name):
    m=re.search(r'constexpr (?:float|int) '+re.escape(name)+r'\s*=\s*([-\d.]+)F?;',CPP)
    if not m: raise ValueError(name)
    return float(m[1])
R=const('kPlayerRadius'); CYL=const('kPlayerHalfHeight')-R
STEP=const('kStepMaximumHeight'); THETA=math.asin(7.4/20); C=math.cos(THETA); T=math.tan(THETA)
P=(6,.6,-90); TIP_Z=-90-20*C
ANGLES=[math.asin((h-.6)/20) for h in (7.53,8,8.20)]
CHEEK_OFFSET=.25

def rot(q):
    x,y,z,w=q; norm=math.sqrt(sum(v*v for v in q)); x,y,z,w=[v/norm for v in q]
    return [[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
            [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
            [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]
def box(name,p,h,q=(0,0,0,1),source='exported'):
    rm=rot(q); wh=[sum(abs(rm[i][j])*h[j] for j in range(3)) for i in range(3)]
    return dict(name=name,p=list(p),h=list(h),q=list(q),r=rm,source=source,
                bounds=[v for i in range(3) for v in (p[i]-wh[i],p[i]+wh[i])])
boxes=[]
for line_no,line in enumerate(SOLID.splitlines(),1):
    m=re.match(r'    \{([^}]+)\},\s*// (.*)',line)
    if not m:continue
    try:v=[float(x.strip().rstrip('F')) for x in m[1].split(',')]
    except ValueError:continue
    if len(v)==10:boxes.append(box(m[2],v[:3],v[7:],v[3:7],f'world_solids.inc:{line_no}'))
# MIRROR parts deliberately excluded by exporter. Recreate the relevant first
# two levels directly from native build_stack formulas and read its constants.
cx=const('kStackCenterX'); cz=const('kStackCenterZ'); half=const('kStackHalfExtent')
band_depth=const('kStackDeckBandDepth'); band=half-band_depth/2; inner=half-band_depth
level_h=const('kStackLevelHeight'); deck_h=const('kStackDeckHalfThickness')
well_start=const('kStackStairwellStart'); well_half=const('kStackStairwellHalfWidth')
col=const('kStackColumnHalf')
boxes.append(box('NativeGrade',(0,-.5,-60),(240,.5,240),source='simulation.cpp:1599'))
for lev in (1,2):
    y=lev*level_h-deck_h; ws=1 if (lev-1)%2==0 else -1
    for side in (-1,1):
        if side!=ws:
            boxes.append(box('NativeStackDeck',(cx,y,cz+side*band),(half,deck_h,band_depth/2),source='native build_stack constants'))
        else:
            spans=[(-half,well_start,band-band_depth/2,band+band_depth/2),
                   (inner,half,band-band_depth/2,band+band_depth/2),
                   (well_start,inner,band-band_depth/2,band-well_half),
                   (well_start,inner,band+well_half,band+band_depth/2)]
            for a,b,z0,z1 in spans:
                boxes.append(box('NativeStackDeck',(cx+ws*(a+b)/2,y,cz+side*(z0+z1)/2),
                                 (abs(b-a)/2,deck_h,abs(z1-z0)/2),source='native build_stack constants'))
        boxes.append(box('NativeStackDeck',(cx+side*band,y,cz),(band_depth/2,deck_h,inner),source='native build_stack constants'))
for lev in (0,1):
    for sx in (-1,1):
        for sz in (-1,1):
            boxes.append(box('NativeColumn',(cx+sx*half,(lev+.5)*level_h,cz+sz*half),(col,level_h/2,col),source='native build_stack constants'))
        boxes.append(box('NativeColumn',(cx+sx*half,(lev+.5)*level_h,cz),(col,level_h/2,col),source='native build_stack constants'))
        boxes.append(box('NativeColumn',(cx,(lev+.5)*level_h,cz+sx*half),(col,level_h/2,col),source='native build_stack constants'))
    pitch=math.atan2(level_h,2*inner); side=1 if lev%2==0 else -1
    boxes.append(box('NativeStair',(cx+side*.18*math.sin(pitch),lev*level_h+level_h/2-.18*math.cos(pitch),cz+side*band),
                     (math.hypot(2*inner,level_h)/2,.18,const('kStackRampHalfWidth')),
                     (0,0,math.sin(side*pitch/2),math.cos(pitch/2)),source='native build_stack constants'))

def overlap(a,b):return all(a[i]<=b[i+1] and a[i+1]>=b[i] for i in (0,2,4))
def local(v,b):
    d=[v[i]-b['p'][i] for i in range(3)]
    return [sum(b['r'][j][i]*d[j] for j in range(3)) for i in range(3)]
def segment_box_distance(a,b,h):
    """Exact piecewise quadratic minimization of segment to axis-aligned box."""
    d=[b[i]-a[i] for i in range(3)]; cuts=[0.,1.]
    for i in range(3):
        if abs(d[i])>1e-12:
            for s in (-h[i],h[i]):
                t=(s-a[i])/d[i]
                if 0<t<1:cuts.append(t)
    cuts=sorted(set(cuts)); candidates=cuts[:]
    for lo,hi in zip(cuts,cuts[1:]):
        mid=(lo+hi)/2; A=B=0.
        for i in range(3):
            v=a[i]+mid*d[i]
            if abs(v)>h[i]:
                offset=a[i]-(h[i] if v>0 else -h[i]); A+=d[i]**2; B+=offset*d[i]
        if A>0:candidates.append(max(lo,min(hi,-B/A)))
    return math.sqrt(min(sum(max(0,abs(a[i]+t*d[i])-h[i])**2 for i in range(3)) for t in candidates))
def capsule_clearance(p,b):
    a=(p[0],p[1]-CYL,p[2]); c=(p[0],p[1]+CYL,p[2])
    return segment_box_distance(local(a,b),local(c,b),b['h'])-R

def bridge_y(z,theta):return .6+(-z-90)*math.tan(theta)
def lower_y(z):return .6+(-z-90)*T
def upper_y(z):return 8-CHEEK_OFFSET-(z-TIP_Z)*T

def sample_line(a,b,n=60):
    return [tuple(a[j]+i/n*(b[j]-a[j]) for j in range(len(a))) for i in range(n+1)]
route_cases=[]
proposed_route_samples=[]
for theta in ANGLES:
    # Route points are soles on proposed walking planes. Capsule centres use
    # .55+.35/cos(slope), accounting for a vertical capsule on an incline.
    parts=[]
    def add(name,a,b,normal_y=1):
        parts.append((name,[(x,y+CYL+R/normal_y,z) for x,y,z in sample_line(a,b)]))
    add('grade approach',(1.94,0,-86),(1.94,.6,-90),math.cos(math.atan2(.6,4)))
    add('lower side cheek',(1.94,.6,-90),(1.94,lower_y(-91.1),-91.1),C)
    add('lower lateral step',(1.94,lower_y(-91.1),-91.1),(6,bridge_y(-91.1,theta),-91.1),min(C,math.cos(theta)))
    mid=TIP_Z+1.5
    add('bridge walk',(6,bridge_y(-91.1,theta),-91.1),(6,bridge_y(mid,theta),mid),math.cos(theta))
    add('upper lateral step',(6,bridge_y(mid,theta),mid),(9.06,upper_y(mid),mid),min(C,math.cos(theta)))
    add('upper side cheek',(9.06,upper_y(mid),mid),(9.06,7.75,TIP_Z),C)
    add('receiver after 0.25m step',(9.06,8,TIP_Z-.1),(9.06,8,TIP_Z-4))
    run=124+TIP_Z-4
    add('onward incline',(9.06,8,TIP_Z-4),(9.06,11,-124),math.cos(math.atan2(3,run)))
    add('native ring endpoint',(9.06,11,-124),(9.06,11,-125.2))
    hits=[]; closest_non_support=None
    for name,points in parts:
        proposed_route_samples.extend((name,p) for p in points)
        for i,p in enumerate(points):
            cap=[p[0]-R,p[0]+R,p[1]-CYL-R,p[1]+CYL+R,p[2]-R,p[2]+R]
            for b in boxes:
                if not overlap(cap,b['bounds']):continue
                clearance=capsule_clearance(p,b)
                # Expected supporting native floor contacts at the last part.
                support=b['name'] in ('NativeGrade','NativeStackDeck') and clearance>=-0.002
                if clearance < -.002:
                    hits.append(dict(segment=name,sample=i,part=b['name'],source=b['source'],penetration_m=-clearance,centre=p))
                elif not support and (closest_non_support is None or clearance<closest_non_support['clearance_m']):
                    closest_non_support=dict(segment=name,part=b['name'],clearance_m=clearance)
    # A broad entire corridor of centres, with .42m end margin: no precision
    # alignment at the tip is needed. This checks surface differences only.
    corridor_z=[TIP_Z+.50+i/100*2 for i in range(101)]
    differences=[upper_y(z)-bridge_y(z,theta) for z in corridor_z]
    tipz=-90-20*math.cos(theta)
    pin=(6,.6-5*math.sin(theta)+2.8*math.cos(theta),-90+5*math.cos(theta)+2.8*math.sin(theta))
    route_cases.append(dict(tip_y=.6+20*math.sin(theta),theta_deg=math.degrees(theta),tip_z=tipz,
        upper_exit_centre_z_range=[min(corridor_z),max(corridor_z)],
        upper_step_up_max_m=max(0,max(differences)),upper_step_margin_m=STEP-max(0,max(differences)),upper_drop_max_m=max(0,-min(differences)),
        upper_tip_centre_margin_min_m=min(corridor_z)-tipz,
        lower_step_delta_m=lower_y(-91.1)-bridge_y(-91.1,theta),
        pan_pin=pin,pan_bottom_y=pin[1]-1.1,auxiliary_pan_pin_y=pin[1]+1.2,
        source_capsule_penetrations=hits,closest_broadphase_non_support=closest_non_support))
# Existing hulls: all outside proposed route/mechanism region by AABB.
htext=SOLID.split('constexpr float kWorldSolidHullPoints[] = {')[1].split('};')[0]
htext=re.sub(r'//[^\n]*','',htext)
v=[float(x) for x in re.findall(r'-?\d+\.\d+(?=F)',htext)]; pts=list(zip(v[::3],v[1::3],v[2::3]))
hulls=[]
for start,num,name in re.findall(r'\{(\d+), (\d+)\},\s*// ([^\n]*)',SOLID.split('constexpr WorldSolidHull kWorldSolidHulls[] = {')[1]):
    pp=pts[int(start):int(start)+int(num)]; bounds=[q for k in range(3) for q in (min(p[k] for p in pp),max(p[k] for p in pp))]
    if overlap([-.9,11,0,13,-125.7,-76],bounds):hulls.append(dict(name=name,bounds=bounds))
keepouts={
 'rack_reservation_only':dict(bounds=[3.7,8.3,2.3,6.5,-82.7,-76.5],status='Chosen envelope; pipe transfer and gate unverified'),
 'pan_sweep_reservation':dict(bounds=[3.7,8.3,min(c['pan_bottom_y'] for c in route_cases),3.5,-87.25,max(c['pan_pin'][2] for c in route_cases)+2.25],status='Level pan outer4.6x4.5; four-bar physical contacts unverified'),
 'outboard_release_prop':dict(bounds=[.2,3.5,0,3.8,-85.35,-84.65],status='Release-agent proposal, pending exact articulated sweep'),
 'main_tail_arm':dict(bounds=[3.1,3.7,.25,3.7,-90.3,-84.0],status='Outboard main arm X3.4; clears pan outerX3.7 only at boundary; exact section clearance unresolved'),
 'aux_tail_arm':dict(bounds=[8.3,8.9,1.45,4.9,-90.3,-84.0],status='Outboard auxiliary arm X8.6; vertical1.2m pin separation; exact section clearance unresolved'),
 'rack_gate_sweep':dict(bounds=[3.7,8.3,2.3,6.5,-84.4,-81.0],status='Overhead hinged gate proposal; overlap with pan envelope is not proof of actual clearance'),
 'rack_gate_retainer':dict(bounds=[8.7,14.3,2.1,2.7,-87.6,-82.0],status='Outboardeast retainer atY2.4 folds towardX14; assumedsection.6 high; native clearance pending')}
# Release view/access route goes west of the folding prop, then to grade ramp.
access=[(6,.9,-25),(10,.9,-75),(10,.9,-82),(10,.9,-75),(-.6,.9,-75),(-.6,.9,-86),(1.94,.9,-86)]
# Return around the source rack's south end, then approach from west.
# The earlier direct shortcut crossed the pan sweep and was rejected.
access_hits=[]
for a,b in zip(access,access[1:]):
 for p in sample_line(a,b):
  for name,k in keepouts.items():
   bb=k['bounds']; ob=box(name,[(bb[i]+bb[i+1])/2 for i in (0,2,4)],[(bb[i+1]-bb[i])/2 for i in (0,2,4)])
   if capsule_clearance(p,ob)<-.002: access_hits.append(dict(name=name,centre=p))
walk_keepout_hits=[]
for segment,p in proposed_route_samples:
 for name,k in keepouts.items():
  bb=k['bounds']; ob=box(name,[(bb[i]+bb[i+1])/2 for i in (0,2,4)],[(bb[i+1]-bb[i])/2 for i in (0,2,4)])
  if capsule_clearance(p,ob)<-.002:walk_keepout_hits.append(dict(segment=segment,reservation=name,centre=p))
steps_ok=all(c['upper_step_up_max_m']<=STEP and abs(c['lower_step_delta_m'])<=STEP and c['upper_tip_centre_margin_min_m']>=R for c in route_cases)
checks={
 'route_step_heights_within_source_limit':steps_ok,
 'lateral_gap_less_than_capsule_diameter':.06<2*R,
 'receiver_step_within_source_limit':CHEEK_OFFSET<=STEP,
 'full_corridor_step_margin_at_least_60mm':all(c['upper_step_margin_m']>=.06 for c in route_cases),
 'receiver_step_margin_at_least_100mm':STEP-CHEEK_OFFSET>=.10-1e-9,
 'pan_bottom_remains_above_grade':min(c['pan_bottom_y'] for c in route_cases)>0,
 'standing_capsule_no_existing_solid_penetrations_at_sampled_points':all(not c['source_capsule_penetrations'] for c in route_cases),
 'no_source_hulls_in_region':not hulls,
 'release_access_no_reserved_body_intersections_at_sampled_points':not access_hits,
 'walk_route_no_reserved_body_intersections_at_sampled_points':not walk_keepout_hits}
report=dict(status='DESIGN PROPOSAL — NOT IMPLEMENTED',method='Analytic surface, exact segment/OBB distance at sampled positions, exported hull AABB exclusion. Does not prove continuous sweeps, native walking, pipe transfer or articulated-body contact.',
 source_commit=subprocess.check_output(['git','-c','safe.directory='+str(ROOT),'rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
 source_sha256={f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in ['src/sim/simulation.cpp','src/sim/world_solids.inc','godot/presentation/solid_export.gd']},
 source_capsule=dict(radius_m=R,cylinder_half_height_m=CYL,step_limit_m=STEP),
 proposed=dict(pivot_surface=P,deck_dimensions_m=[3,.4,20],nominal_angle_deg=math.degrees(THETA),nominal_tip_z=TIP_Z,
 receiver_bounds=[7.56,10.56,7.6,8,TIP_Z-4,TIP_Z],receiver_clear_area_m=[3,4],
 upper_cheek_offset_m=CHEEK_OFFSET,upper_cheek_top_y_range=[upper_y(TIP_Z+3),upper_y(TIP_Z)],
 upper_cheek_z_range=[TIP_Z,TIP_Z+3],lower_cheek_z_range=[-90-2*C,-90],
 lower_cheek_x_range=[.44,4.44],grade_ramp_x_range=[.44,3.44],grade_ramp_z_range=[-90,-86],
 onward_run_m=124+TIP_Z-4,onward_slope_deg=math.degrees(math.atan2(3,124+TIP_Z-4)),
 onward_native_endpoint=[9.06,11,-125.2],lateral_gap_m=.06,
 release_handle=[10,0,-82],observer=[1,0,-82]),
 stop_band_cases=route_cases,keepouts=keepouts,access_path_centres=access,access_reserved_intersections=access_hits,
 walking_path_reserved_intersections=walk_keepout_hits,source_hull_intersections=hulls,checks=checks,all_analytic_checks_pass=all(checks.values()),
 unresolved=['Native capsule sweeps and actual normal-input traversal',
 'Actual pan/four-bar articulated body collision and load paths',
 'Rack-to-pan gate, lane thresholds and rolling transfer',
 'Receiver structural seats and complete arrest force law',
 'Player early boarding and contact reaction',
 'First-person visual proof and Android execution'])
(OUT/'geometry-statistics.json').write_text(json.dumps(report,indent=2)+'\n')
# Engineering SVG: side elevation above, plan below. No generated art.
svg=[]
def add(s):svg.append(s)
def esc(s):return str(s).replace('&','&amp;').replace('<','&lt;').replace('>','&gt;')
def txt(x,y,s,size=14,color='#263447',weight='normal'):
 add(f'<text x="{x:.2f}" y="{y:.2f}" font-size="{size}" fill="{color}" font-weight="{weight}">{esc(s)}</text>')
def line(x1,y1,x2,y2,color='#607080',width=2,dash=''):
 add(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{color}" stroke-width="{width}"'+(f' stroke-dasharray="{dash}"' if dash else '')+'/>')
def rect(x,y,w,h,fill,stroke='#526277',opacity=1):
 add(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="{fill}" stroke="{stroke}" opacity="{opacity}"/>')
def poly(points,fill,stroke='#526277',opacity=1):
 add(f'<polygon points="'+ ' '.join(f'{x:.2f},{y:.2f}' for x,y in points)+f'" fill="{fill}" stroke="{stroke}" opacity="{opacity}"/>')
add('<svg xmlns="http://www.w3.org/2000/svg" width="1360" height="1190" viewBox="0 0 1360 1190"><style>text{font-family:Arial,sans-serif}</style>')
rect(0,0,1360,1190,'#f5f7fa','none');txt(38,38,'SCRAPERX · Pipe-loaded balance bridge',26,weight='bold')
txt(38,64,'DESIGN PROPOSAL — NOT IMPLEMENTED · Metres · Source-linked geometry screening, not native walking proof',15,'#9e451c',weight='bold')
txt(38,98,'SIDE ELEVATION  /  looking east, toward +X',17,weight='bold')
SX=lambda z:55+(-z-74)*23
SY=lambda y:414-y*23
for y in (0,4,8,11):
 line(55,SY(y),1260,SY(y),'#d1d9e1',1,'5 5');txt(1268,SY(y)+5,f'+{y}m',12)
line(SX(-74),SY(0),SX(-126),SY(0),'#42556a',3)
# Initial and target deck surfaces and slab.
line(SX(-90),SY(.6),SX(-110),SY(.6),'#98a5b3',6,'7 5')
for theta in (ANGLES[0],ANGLES[2]):
 line(SX(-90),SY(.6),SX(-90-20*math.cos(theta)),SY(.6+20*math.sin(theta)),'#d99436',1.5,'4 4')
pts=[]
for s,n in ((0,0),(20,0),(20,-.4),(0,-.4)):
 z=-90-s*C+n*math.sin(THETA); y=.6+s*math.sin(THETA)+n*C;pts.append((SX(z),SY(y)))
poly(pts,'#3f7695','#28526e')
# receiver and connector, projected from side X; distinguish dashed/green.
line(SX(TIP_Z+3),SY(upper_y(TIP_Z+3)),SX(TIP_Z),SY(7.75),'#248468',6)
line(SX(TIP_Z),SY(8),SX(TIP_Z-4),SY(8),'#248468',7)
line(SX(TIP_Z-4),SY(8),SX(-124),SY(11),'#248468',6)
line(SX(-124),SY(11),SX(-126),SY(11),'#37414d',8)
for z,top in ((TIP_Z-1,8),(TIP_Z-3.4,8),(-119,8+3*((-119)-(TIP_Z-4))/(-124-(TIP_Z-4)))):
 line(SX(z),SY(0),SX(z),SY(top-.2),'#91a49c',4)
# Raised tail four-bar, initial and target.
for theta,color,dash in ((0,'#96a3b2','5 4'),(THETA,'#775b9b','')):
 pin_y=.6-5*math.sin(theta)+2.8*math.cos(theta);pin_z=-90+5*math.cos(theta)+2.8*math.sin(theta)
 for off in (0,1.2):line(SX(-90),SY(.6+off),SX(pin_z),SY(pin_y+off),color,4,dash)
 line(SX(pin_z),SY(pin_y),SX(pin_z),SY(pin_y+1.2),color,3,dash)
 floor=pin_y-1.0
 rect(SX(pin_z+2.25),SY(floor+.9),4.5*23,.9*23,'#ccb28a',color,.4 if theta==0 else .75)
 # A level-pan outline is an envelope, not allsolid interior.
 line(SX(pin_z+2.25),SY(floor),SX(pin_z-2.25),SY(floor),color,4,dash)
rack_end_y=2.4+6.2*math.tan(math.radians(5))
line(SX(-82.7),SY(2.4),SX(-76.5),SY(rack_end_y),'#ab7335',6)
for i in range(5):
 z=-81.9+i*1.05;y=2.4+(z+82.7)*math.tan(math.radians(5))+.4
 add(f'<circle cx="{SX(z):.2f}" cy="{SY(y):.2f}" r="9.2" fill="#c2ccd6" stroke="#50667a" stroke-width="3"/>')
line(SX(-90),SY(0),SX(-90),SY(1.8),'#775b9b',5)
txt(62,451,'Rack + shallow pan remain above grade; vertical 1.2m four-bar separation is a proposal.',13)
txt(60,150,'20 pipes · 800kg each',13);txt(60,169,'Four lanes × five; source transfer unverified',12)
txt(445,300,'Pivot walking surface +0.6m',13)
txt(600,210,'20m deck · 3m usable width',15,weight='bold')
txt(775,280,'Stress band: tip +7.53…+8.20m',12,'#a25d11')
txt(857,177,'Side cheek ends +7.75',13,'#17684e');txt(868,196,'0.25m step to +8 dock',13,'#17684e')
txt(1090,130,'Existing +11 ring',13,weight='bold')
# PLAN, scale12m: Z more negative downward (north/tower).
txt(38,503,'PLAN  /  walking supports and reserved moving volumes',17,weight='bold')
PX=lambda x:135+(x+2)*17
PZ=lambda z:535+(-z-75)*11.7
for z in (-80,-90,-100,-110,-120):
 line(110,PZ(z),450,PZ(z),'#d4dbe3',1,'4 4');txt(55,PZ(z)+4,f'Z {z}',12)
for x in (0,6,10):txt(PX(x)-12,526,f'X{x}',12)
def plan_box(x0,x1,z0,z1,fill,stroke='#526277',opacity=1):rect(PX(x0),PZ(z1),(x1-x0)*17,(z1-z0)*11.7,fill,stroke,opacity)
plan_box(3.7,8.3,-82.7,-76.5,'#e9c99c','#ab7335',.6)
plan_box(3.7,8.3,-87.25,-82.05,'#d1c3e0','#775b9b',.6)
plan_box(.2,3.5,-85.35,-84.65,'#eeb39f','#a04c36',.8)
plan_box(3.1,3.7,-90.3,-84.0,'#b4a0cc','#775b9b',.65)
plan_box(8.3,8.9,-90.3,-84.0,'#b4a0cc','#775b9b',.65)
plan_box(8.7,14.3,-87.6,-82.0,'#eeb39f','#a04c36',.2)
plan_box(.44,3.44,-90,-86,'#a9d5c6','#248468')
plan_box(.44,4.44,-90-2*C,-90,'#91c4b0','#248468')
plan_box(4.5,7.5,TIP_Z,-90,'#a4c5d8','#28526e')
plan_box(7.56,10.56,TIP_Z,TIP_Z+3,'#91c4b0','#248468')
plan_box(7.56,10.56,TIP_Z-4,TIP_Z,'#76b29b','#248468')
plan_box(7.56,10.56,-124,TIP_Z-4,'#a9d5c6','#248468')
plan_box(5.6,11.5,-126.4,-124,'#a8afb8','#37414d')
path=[(1.94,-86),(1.94,-91.1),(6,-91.1),(6,TIP_Z+1.5),(9.06,TIP_Z+1.5),(9.06,-125.2)]
for a,b in zip(path,path[1:]):line(PX(a[0]),PZ(a[1]),PX(b[0]),PZ(b[1]),'#e35839',2,'5 3')
for a,b in zip(access[2:],access[3:]):line(PX(a[0]),PZ(a[2]),PX(b[0]),PZ(b[2]),'#e35839',2,'5 3')
add(f'<circle cx="{PX(10):.1f}" cy="{PZ(-82):.1f}" r="6" fill="#e35839"/>')
labels=[(3.7,-78,'RACK: 4 lanes; handle at X10 / Z−82'),(3.7,-84,'PAN SWEEP: keep player outside'),(.2,-85.2,'Outboard release prop folds west'),(.44,-88,'GRADE APPROACH'),(.44,-91,'LOWER CHEEK'),(4.5,-98,'MOVING DECK'),(7.56,-106,'UPPER SIDE CHEEK'),(7.56,-111,'FIXED +8 DOCK'),(7.56,-118,'ONWARD TO +11'),(7.56,-125,'NATIVE TOWER RING')]
for i,(x,z,label) in enumerate(labels):
 yy=PZ(z);line(PX(11),yy,500,yy,'#9ba8b5',1);txt(512,yy+4,label,13,weight='bold' if i in (3,5,7,9) else 'normal')
txt(890,548,'SCREENING RESULTS',16,weight='bold')
y=578
for key,value in checks.items():
 label=key.replace('_',' ')
 # concise wrapping of long result names.
 words=label.split();rows=[];row=''
 for word in words:
  if len(row)+len(word)>40:rows.append(row);row=word
  else:row=(row+' '+word).strip()
 if row:rows.append(row)
 txt(890,y,('PASS  ' if value else 'REVIEW  ')+rows[0],12,'#17684e' if value else '#b34426');y+=17
 for row in rows[1:]:txt(936,y,row,12);y+=16
 y+=12
for label in ['Limit: sampled positions, not continuous sweeps.', 'No native walking / Jolt contacts were run.', 'Rack transfer and pan links remain unverified.', 'Only proposed access route is screened.', 'Orange dashes: proposed player route.', 'Purple / tan: reserved moving/source volumes.']:
 txt(890,y,label,12,'#455367');y+=23
maxup=max(c['upper_step_up_max_m'] for c in route_cases);maxdown=max(c['upper_drop_max_m'] for c in route_cases)
txt(890,1105,f'Upper side step: rise {maxup:.3f}m / drop {maxdown:.3f}m.',13)
txt(890,1128,f'Pan minimum bottom clearance: {min(c["pan_bottom_y"] for c in route_cases):.3f}m.',13)
txt(38,1174,'Generated from check_bridge_geometry.py + current production world_solids.inc. Check JSON for hashes, assumptions and limits.',12,'#5e6b7b')
add('</svg>');(OUT/'bridge-layout.svg').write_text('\n'.join(svg)+'\n')
print(json.dumps(dict(checks=checks,stop_band_cases=[{k:c[k] for k in ('tip_y','theta_deg','upper_step_up_max_m','upper_drop_max_m','upper_tip_centre_margin_min_m','pan_bottom_y','source_capsule_penetrations')} for c in route_cases],access_intersection_count=len(access_hits),artifacts=[str(OUT/'geometry-statistics.json'),str(OUT/'bridge-layout.svg')]),indent=2))

raise SystemExit(0 if report["all_analytic_checks_pass"] else 1)
