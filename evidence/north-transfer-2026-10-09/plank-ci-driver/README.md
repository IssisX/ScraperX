# Bounded fracture-recovery driver correction

Previous exact-source x86 CI37995212805 failed because the final player centreX=-4.6869 lay outside the real steel tray endX=-4.8. Original driver never braked fragment-impartedX momentum. Checkpoint footing correctly rejected that edge perch.

The final driver preserves the original ordinaryZ-only fragment escape until actual steel1936 contact on the clearZ=-127.2 strip. It then uses bounded two-axis input toward(-6.8,-127.2) and requires actual firm footing, horizontal speed<0.1m/s and interior position before the unchanged180tick neutral settle and persistent-damage assertion. No native physics, strength or checkpoint criterion changed.

An initial candidate appliedX steering too early and dilutedZ escape; it failed onARM by falling past the tray. Its actual failing log is retained. The corrected sequence passed the same native impact path onARM at557ticks, real1936,gravity1,deaths0,brokenmask112. This does not prove x86 closure; exact-source Actions remains pending.

Commands actually executed:

```
proot-distro login ubuntu -- cmake --build /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/host-build --target scraperx_deformable_plank_player_tests -j 2
proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/host-build/scraperx_deformable_plank_player_tests impact
```
