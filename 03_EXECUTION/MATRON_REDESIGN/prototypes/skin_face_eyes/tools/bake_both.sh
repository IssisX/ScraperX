#!/bin/bash
cd /tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/overhaul/skin_face_eyes/tools
rm -f ../work/s2_real.log ../work/s2_wrong.log
timeout 1200 nice python3 -I stage2_tex.py 2048 real > ../work/s2_real.log 2>&1
timeout 1200 nice python3 -I stage2_tex.py 2048 wrong > ../work/s2_wrong.log 2>&1
echo BAKE_FINISHED; tail -n 1 ../work/s2_real.log ../work/s2_wrong.log
