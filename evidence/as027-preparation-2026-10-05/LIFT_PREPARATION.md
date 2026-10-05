# Isolated large-lift design preparation (not integrated)

Owner target:1000+m structure; one next slice20–30m useful lift travel.
Seven original C++ source files reconstructed from supplied task text match
expected lengths/SHA256. Full ZIP was not materialized in this executor.

CHOSEN screening topology: two18m scissor stages,15→65degrees.
DERIVED travel=36*(sin65-sin15)=23.3096m. Horizontal stroke9.780m.
Provisional effective lifted mass2475kg gives gravitywork566kJ and starting
horizontal force181kN. Masses/material sections and drive budget need native
validation;200kN is a screening choice, not an accepted forceboost.
At0.08m/s carriage rate, stroke lasts122s (likely too slow for gameplay).
Do not ship a finite-force actuator as though it implied a finite-power source.

CHOSEN placement candidate: yaw+pi/2,origin(-31,108.732514,-153), assuming
lowerlocalpivotY2 and decktopoffset0.95. Walking surface121→144.3096,
playercenter121.9→145.2096. Conservative envelopeX[-33.8,-28.2],Y[108,147],
Z[-172,-151.5]. Read-only sourceAABB screen found no generatedbox overlap;
actual native sweeps, guide/bracing/player clearance remain unproven.
Proposed entry121 and receiver143: newtonguesX[-29,-25.75],Z[-162.7,-161.3],
0.2m gap todeckedgeX-29.2. Raised departure is1.3096m down to143receiver,
thenactual143ring. Base needsvisiblecantilever bearing at110, notfloatingbase.
AS026 atX3..12.5 remainsremote fromwestenvelope.

Needs: finite drive/work source, visibleguide andjointreactionpaths, joint/CCD
andpairwiseexclusions, eccentric85kgrider loading, loss-of-power physical arrest,
return/checkpoint, realplayer boarding/departure/contact momentum, bounded
load/refinement probes and mobile measurement. Approximately9dynamicbodies plus
brake/guidebodyneeds; no Fold performanceclaim. No productionlift files edited.
