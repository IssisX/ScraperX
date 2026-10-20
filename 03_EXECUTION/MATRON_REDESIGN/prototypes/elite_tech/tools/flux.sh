#!/bin/bash
# usage: flux.sh SEED OUTFILE "PROMPT"  -- calls the public FLUX.1-schnell (Apache-2.0) Space's Gradio API
set -e
S=${SPACE:-https://black-forest-labs-flux-1-schnell.hf.space}/gradio_api/call/infer
BODY=$(python3 -I -c 'import json,sys; print(json.dumps({"data":[sys.argv[2], int(sys.argv[1]), False, 1024, 1024, 4]}))' "$1" "$3")
EID=$(curl -sS -X POST "$S" -H 'Content-Type: application/json' -d "$BODY" | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["event_id"])')
echo "event $EID"
OUT=$(curl -sS -N --max-time 240 "$S/$EID")
echo "$OUT" | tail -c 600
URL=$(echo "$OUT" | grep '^data:' | tail -1 | sed 's/^data: //' | python3 -I -c 'import json,sys; d=json.load(sys.stdin); print(d[0]["url"])')
curl -sS -o "$2" "$URL"
ls -la "$2"
