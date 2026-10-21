extends RefCounted

# Authored files and conversation. Guide remains the only speech queue.
const KEYS := [&"handover_record_assurance", &"handover_ro_denial", &"handover_iona_revision", &"handover_ro_witness", &"handover_iona_gallery"]
const SEND_KEYS := [&"handover_ro_send_scope", &"handover_dev_received"]
const ARI_KEYS := [&"handover_ro_ari_first", &"handover_iona_ari_first"]
const CUES := {
	&"handover_record_assurance": "No occupied areas remain within the handover boundary.",
	&"handover_ro_denial": "I didn't record that.",
	&"handover_iona_revision": "Your survey says access withdrawn. Occupancy not assessed. They changed the conclusion and kept your name.",
	&"handover_ro_witness": "Who delivered the final handover?",
	&"handover_iona_gallery": "Ari Ward. Barrel Helix landing, two hundred and fifty-three metres. Find them. Ask what they were told.",
	&"handover_source_survey": "Temporary construction access withdrawn. Occupancy not assessed.",
	&"handover_ro_send_scope": "Send both versions to Dev. Let him see exactly what they changed.",
	&"handover_dev_received": "Dev Arlen. Both versions received. This proves the statement was changed. It doesn't tell me what the return is carrying. Don't reconnect anything on the strength of a fucking signature.",
	&"handover_ro_ari_first": "Keep both versions. I want Ari's account before Dev gets another version of it.",
	&"handover_iona_ari_first": "Then ask Ari what they actually saw. Not what they were told to say."
}
const SPEAKERS := {
	&"handover_record_assurance": "RECORD", &"handover_ro_denial": "RO",
	&"handover_iona_revision": "IONA", &"handover_ro_witness": "RO",
	&"handover_iona_gallery": "IONA", &"handover_source_survey": "SOURCE",
	&"handover_ro_send_scope": "RO", &"handover_dev_received": "DEV",
	&"handover_ro_ari_first": "RO", &"handover_iona_ari_first": "IONA"
}
