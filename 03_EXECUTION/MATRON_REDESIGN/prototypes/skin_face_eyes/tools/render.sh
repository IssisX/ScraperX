#!/bin/bash
# usage: render.sh OUTDIR SHOTS [VAR=val ...]
T=/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/overhaul/skin_face_eyes
OUT=$1; SHOTS=$2; shift 2
mkdir -p $OUT
env "$@" OUTDIR=$OUT SHOTS=$SHOTS DISPLAY=:0 timeout 150 nice /home/user/ScraperX/tools/godot/godot --path $T/probe --rendering-method gl_compatibility --resolution 512x512 2>&1 | grep -v "^$" | tail -25
