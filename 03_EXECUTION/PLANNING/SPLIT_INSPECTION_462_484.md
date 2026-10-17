# Next bounded encounter —462–484m split inspection crosshead

Status: design correction only; no gameplay implementation/runtime pass. Production starts after currentcatch deliveryGREEN. Fullscope unchanged; rootowns engineering/integration.

## Corrected live anchors

Targetsource2a05fb4 `src/sim/world_solids.inc:1154–1178`; native quaternion construction `simulation.cpp:3314` normalizes and applies the listed rotations directly.

The earlier plan reversed the leftbrace rise. Quaternion z=-.2347,w=.9721, center(-10.725,467.5,-128.55), half-length12.053 produces approximately **(0,462,-128.55)→(-21.45,473,-128.55)**. The high473m point is the westcolumn, notx0. The prior x0/473 crosshead and negativeX uppergirder proposal are superseded before implementation; do not attempt to manufacture an approach to them.

- Earned entry: westroof(-19.5,462.25,-142), then real462m northroof to the lowbrace toe nearx0.
-462m northroof: X[-21.45,21.45],Z[-132.55,-128.55],top462.25.
-West473m column: centerX=-21.45,Z=-128.55, halfwidth.66, top473; actual inclinedbrace approaches its inneredge. Finalbrace→crosshead transfer may require an ordinary supportedJump/real mantle; it is not proven walkable.
-484m northroof: X[-20.54,20.54],Z[-133.46,-129.46],top484.25; realWorldSolid51 is a semanticentity, never aKit index.

## Selected arrangement —CHOSEN, unverified

Follow the existingleftbrace up to the outer473m column. A small inspectioncrosshead gives room to observe the inward gap and prepare sideways movement. Jump south across the realgap, brake on a separate receivingpad, turn east, and follow a narrowascending trussedgirder into the484m roof.

- Takeoffcrosshead: X[-21.5,-18.0],Z[-130.2,-128.15],top473.25. Bind visible bearers to the actualcolumn; evaluate capsule/head clearance and the .25m footing transfer.
- Physicalgap: Z[-132.3,-130.2],2.1m clear, over actual462m roof for a short committedmiss. No invisiblecollision spans it.
- Receiver: X[-20.7,-18.0],Z[-134.6,-132.3],top473.25. Its2.3m depth requires braking before turning onto the girder. Overshooting into the inner shaft remains consequential.
- Ascendinggirder: begin at receiver's eastedgeX=-18.0, centreZ=-134.15; endX≈10.0 at484.25m beside the roof.28m run/11m rise gives≈21.4° nominalslope; .55m honestvisible walkingwidth. Use visiblysupported/trussed geometry and realnativeParts/mass/reactions. Author endpoint top/centre carefully, with a level receiving shoe X[9.45,10.55],Z[-134.45,-132.65],top484.25 that overlaps the roof by.81m. Walk north over that actual shoe onto the roof.
- Miss/retry: a deliberate shortsouthward jump atX≈-19.3 must land on the real462m northroof, then return via the existingbrace. Falling before the roof footprint or overshooting into the shaft can remain severe.

Independent[Super_Agent source review](SPLIT_INSPECTION_462_484_REVIEW.md) disproved the earlierZ=-133 ascent: soles reach481.95m aroundX4.15, placing a standing1.8m capsule into the roofunderside483.75 before arrival. AtZ=-134.15 the capsule has.34m horizontalclearance; early overlap with the westroof remains high overhead and stillneeds its actualsweep checked. Keep the girderout of the roofvolume; no cutting the floor or reducing capsuleheight.

These are proposalbounds, not observed route behavior. They may be adapted by root to demonstrated contact/clearance/receiving failures without revisiting proven movement parameters.

## Physical prediction and player experience

Player muscle supplies finite takeoff/climb work. Gravity governs flight and misses. Existing5.5m/s vertical takeoff gives approximately1.12s same-height flight; a2.1m sidewaysgap is feasible in principle, while actualapproach/braking/contact need ordinaryinputs. An11m fall plusjumpheight approaches15–16m/s, below the shared18m/s lethalimpact threshold; actual landing orientation/impulse still decides recovery.

Purpose: exposed upperinspection access. Approach teaches the brace and shelteredreceiver; player reads height, commits sideways, brakes, turns, and climbsnarrow steel. Secure484m footing releases tension and prepares the nextascent. Contrast/nosing/materialwear must explain realedges without widening collision unintentionally. Reuse native Kit and existing entity-aware presentation; no newphysics/controller/effectdispatcher.

## Rejecting acceptance

1. Ordinarycontrols reach484m from the earned462m entry through the actualgap; debugfixture only atinitialentry, no later relocation/rootmotion.
2. Shorttransfer loses realcontact, lands on462m support and retries withoutreset. Requireactual receiver/roof contact and no bypass tocredit completion.
3. Parts/collision/render agree on gap, girder and clear exit; preserve pipe/catch/upperroutes and complete one normal exactsourceGREEN delivery.

Open discriminators: actualexistingbrace approach and columntransfer, sidewayslaunch/receiver braking, ascendinggirder support/clearance and shortmiss/retry. Resolve these with the changed route path, not a broadtestcampaign. The earlier Super_Agent advisory was medium/0children and proposal-only; its stairalternative remains unselected.
