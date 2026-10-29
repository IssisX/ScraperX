# Test-reach map generator

Rebuilds `03_EXECUTION/TEST_REACH.md` (which tests a change to a source file or function can reach).

- Needs: Python 3, `pip install libclang` (18.x, matching `clang`), `clang` on the path, and a configured
  build (`build/host`) for the fetched Jolt and godot-cpp headers. If `build/host/compile_commands.json`
  exists (`cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, or `ninja -t compdb`), its flags are used per file;
  without it the flags of the CMake build are built in.
- Run: `tools/test_reach/run.sh` (about a minute). Intermediate files go to `build/test_reach/`.
- Environment: `TEST_REACH_ROOT` (another checkout), `TEST_REACH_BUILD`, `TEST_REACH_OUT`, `COMPILE_COMMANDS`.
- Steps: `cg.py` (libclang call graph and struct-field references), `native.py`/`gen_native.py`/`subject.py` (the
  native groups from `tests/simulation_tests.cpp` main()), `gd.py`/`gd_an.py`/`gen_gd.py` (the touch scenarios and
  GDScript), `assemble.py` (the document).
- Judgment is in two tables: the subject regexes in `subject.py` and the file-to-subject map in `assemble.py`.
- It reads the code only; no test is run. Regenerate when the test lists or the call structure change a lot.
