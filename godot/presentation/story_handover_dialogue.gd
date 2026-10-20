extends RefCounted

# Authored files and conversation. Guide remains the only speech queue.
const KEYS := [&"handover_record_assurance", &"handover_ro_denial", &"handover_iona_revision", &"handover_ro_witness", &"handover_iona_gallery"]
const CUES := {
	&"handover_record_assurance": "No occupied areas remain within the handover boundary.",
	&"handover_ro_denial": "I didn't record that.",
	&"handover_iona_revision": "Your survey says access withdrawn. Occupancy not assessed. They changed the conclusion and kept your name.",
	&"handover_ro_witness": "Who delivered the final handover?",
	&"handover_iona_gallery": "Ari Ward. Barrel Helix landing, two hundred and fifty-three metres. Find them. Ask what they were told.",
	&"handover_source_survey": "Temporary construction access withdrawn. Occupancy not assessed."
}
const SPEAKERS := {
	&"handover_record_assurance": "RECORD", &"handover_ro_denial": "RO",
	&"handover_iona_revision": "IONA", &"handover_ro_witness": "RO",
	&"handover_iona_gallery": "IONA", &"handover_source_survey": "SOURCE"
}
