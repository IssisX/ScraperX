import pickle,json,collections,re,os
from common import R,S
groups,res,direct,defs,names,cutres,core,hubs=pickle.load(open(S+'/native.pkl','rb'))
d=json.load(open(S+'/cg.json'))
ffield=collections.defaultdict(set)
for f,v in d.items():
    for cur,fl in v.get('fields',[]): ffield[cur].add(fl)
ALL='(all: scraperx_sim.athletic_traversal)'
real=[g for g in res if g!=ALL]
gread={g:set().union(*[ffield[u] for u in res[g][1] if u in defs and defs[u][2].startswith('tests/')] or [set()]) for g in res}
SUBJ={
 'Her hold, arm tears, fatality (she grabs, carries, tears, kills)':
   (r'^(MatronSnapshot\.(grabs|grab_misses|carries|dangles|armplays|releases|spares|grips_torn|retakes|arm_reposes|brace_losses|carry_drops|arm_tears|arm_throws|claims|fatal_\w+|reels|armplay_beat|waste_not|hand2|grip_on|hold_braced|hold_s|hand|aim_ledge|aim)|Snapshot\.(held_by_matron|claimed_by_matron|arm_tear_count|left_arm_lost|right_arm_lost|one_arm|runs_ended_by_matron|matron_hand\w*|matron_knee\w*)|FatalRun\.\w+|HoldRun\.\w+)$',
    'matron_hold.cpp, matron_fatality.cpp, physics_world_grab.cpp, physics_world_arm.cpp, physics_world_fatality.cpp, physics_world_one_arm.cpp, matron_grip.hpp'),
 'Her climbing (ladders, mantles, found climbs, journeys)':
   (r'^MatronSnapshot\.(climbing|climbs|clinging|climb_\w+|mantling|foot_power_w|support_n|bracing|trap|traps|contact_\w+)$|^MatronJourney\.\w+|^MatronContactAudit\.\w+',
    'matron_climb.cpp, matron_traversal.cpp, matron_routecraft.cpp, matron_found_climb.cpp, matron_journey.cpp, matron_travel.cpp, matron_contact.cpp'),
 'Her senses and hunt (hearing, sight, lead, fooled, wary)':
   (r'^\w+\.(noise_heard|sees_player|fooled|wary|lead_\w+|acoustic_queries|hushed|creaks|target_x|exposure)$',
    'matron.cpp (state machine, hunt), matron_moves.cpp'),
 'Her balance and the player\'s blows (strike, stagger, knockdown, rage)':
   (r'^MatronSnapshot\.(shoves|staggers|knockdowns|blow_mps|down_s|down_left_s|fall_x|kneeling|rage|stage)$|^Snapshot\.strike\w*',
    'physics_world_strike.cpp, matron.cpp'),
 'Her corner pipe':
   (r'^MatronSnapshot\.(on_pipe|pipe_\w+)$','matron_pipe.cpp'),
 'Her cable and catwalk (lines, rotten planks)':
   (r'^MatronSnapshot\.(on_line|cable_slides|vaults|leaps|unrigs|planks_fallen|plank_load_peak_n|bolts_popped|catwalk_allowed|crumbs|crumbs_walked)$',
    'matron_line.cpp, matron.cpp'),
 'Her pace and getting unstuck':
   (r'^MatronSnapshot\.(pace|pace_mps|pace_changes|stalls|step_ups|clambers|sidesteps|reroutes|longest_stall_s|stride|speed|turn|turn_s)$',
    'matron_moves.cpp, matron.cpp'),
}
n=len(real)
fc=collections.Counter(f for g in real for f in gread[g])
common={f for f,c in fc.items() if c>0.35*n}
for g in gread: gread[g]-=common
size={g:sum(1 for u in cutres[g] if u in defs and defs[u][2].startswith('src/')) for g in real}
rank={g:i for i,g in enumerate(sorted(real,key=lambda g:(size[g],g)))}
rows=['| Subject | Source files (my reading of their header comments) | Groups whose test code reads a field of that subject (count = distinct fields of that subject read; most first) |','|---|---|---|']
out={}
for sub,(rx,fl) in SUBJ.items():
    r=re.compile(rx)
    hit={g:sorted(f for f in gread[g] if r.match(f)) for g in real}
    hit={g:v for g,v in hit.items() if v}
    gs=sorted(hit,key=lambda g:(len(hit[g]) if False else rank[g]))
    # focus: rank by number of distinct subject fields read, desc
    gs=sorted(hit,key=lambda g:(-len(hit[g]),rank[g]))
    rows.append(f'| {sub} | {fl} | '+', '.join(f'`{g}` ({len(hit[g])})' for g in gs)+' |')
    out[sub]=gs
subj_table='\n'.join(rows)
pickle.dump(subj_table,open(S+'/subj.pkl','wb'))
print(subj_table[:3000])
