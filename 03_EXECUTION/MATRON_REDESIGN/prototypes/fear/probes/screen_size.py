"""How big her face, eyes and a hand read on the Fold's inner screen.

Camera: godot/presentation/ui/settings_store.gd:70 fov 82 (Godot Camera3D
keep_aspect defaults to KEEP_HEIGHT, so 82 degrees is the vertical FOV);
viewport 2160x1856 (godot/project.godot:21-22).
Sizes: face chin-to-crown 0.23 m, eye opening 0.030 m wide, iris 0.012 m,
penlight 0.012 m wide, a shoe 0.26 m long, her hand 0.18 m.
"""
import math

H_PX = 1856
FOV_V = math.radians(82.0)
px_per_m_at_1m = H_PX / (2.0 * math.tan(FOV_V / 2.0))
things = [("face", 0.23), ("eye opening", 0.030), ("iris", 0.012), ("hand", 0.18), ("shoe", 0.26), ("whole figure 1.75 m", 1.75)]
print("px per metre at 1 m: %.0f" % px_per_m_at_1m)
print("%-8s" % "dist m" + "".join("%22s" % t for t, _ in things))
for d in [0.3, 0.5, 0.85, 1.5, 2.0, 3.0, 4.0, 6.0, 9.0, 13.0, 20.0, 30.0]:
    print("%-8.2f" % d + "".join("%22.1f" % (px_per_m_at_1m * s / d) for _, s in things))
