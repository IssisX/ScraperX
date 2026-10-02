# AS-025 — working-mill interior presentation

The normal tower's first ten storeys now have rusted process/return risers and collars, attached valve branches/handwheels, flush grate accents, guarded amber service lights, hazard marks and short slack cable loops. One damaged junction emits three small sparks every 4.2 seconds. These are presentational service details, not new collision or climb authority; the flexible cargo net remains a separate native Jolt body.

The three PNGs were rendered in official Godot 4.7 ARM64 with the compiled native extension and inspected. `interior_inspection.gd.txt` places an inspection camera at standing eye height on existing deck rings, hides HUD and pauses main processing. It proves visual dressing, not player traversal or phone performance. Software compatibility rendering uses LOW/shadows OFF/MSAA OFF and enables 3D only for capture frames. `render.log` records the run. The ordinary touch route separately proves cargo traversal.

Review corrected the warning stripes to face the viewing side, attached handwheels to service pipe branches, and increased the initially millimetre-sized particle scale to visible sparse flecks. The open central shaft and existing route geometry are retained. The owner accepted roughly the first 100 m as sufficient scope for now. Android appearance and device acceptance remain open.
