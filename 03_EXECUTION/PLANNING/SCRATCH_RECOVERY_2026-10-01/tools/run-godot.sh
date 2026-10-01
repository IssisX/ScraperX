#!/bin/sh
exec env DISPLAY=:97 \
  XDG_CACHE_HOME=/tmp/scraperx-runtime/cache \
  XDG_CONFIG_HOME=/tmp/scraperx-runtime/config \
  XDG_DATA_HOME=/tmp/scraperx-runtime/data \
  /tmp/scraperx-runtime/Godot_v4.7-stable_linux.x86_64 "$@"
