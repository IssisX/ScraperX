"""Paths shared by the test-reach scripts (see README.md)."""
import os

# The repository root, or another checkout's (to map an older commit).
R = os.environ.get('TEST_REACH_ROOT') or os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
# The configured build: the Jolt and godot-cpp sources fetched into _deps.
BUILD = os.environ.get('TEST_REACH_BUILD') or os.path.join(R, 'build', 'host')
# Where the call graph and the tables go between steps (not committed).
S = os.environ.get('TEST_REACH_OUT') or os.path.join(R, 'build', 'test_reach')
os.makedirs(S, exist_ok=True)
