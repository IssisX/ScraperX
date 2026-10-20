#!/bin/bash
# usage: run.sh PROBE  (env RES=WxH, TAG=suffix) -- renders the probe under Xvfb :0, gl_compatibility
ET=/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/overhaul/elite_tech
OUTD=$ET/renders/$1${TAG}
mkdir -p $OUTD
PROBE=$1 DATA=$ET/data OUT=$OUTD DISPLAY=:0 timeout ${TMO:-400} nice -n 5 /home/user/ScraperX/tools/godot/godot --path $ET/probe \
  --rendering-method gl_compatibility --resolution ${RES:-768x768} --position 0,0 > $OUTD/run.log 2>&1
echo "exit $?"
grep -E "ADAPTER|METHOD|METRIC|SHOT|ERROR|error|Error|WARNING" $OUTD/run.log | grep -v "^\s*$" | head -60
