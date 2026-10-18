# Physical receiving handoff

AS026 could reach the first corner contact on its114m shelf, then hang above it when onward steering opposed the climb direction. The Cartesian hand target stayed raised while gravity sag left the feet in air. A swept-clear receiving phase now lowers the same bounded hand targets toward the accepted shelf despite subsequent steering. Deliberate Drop still cancels.

The first repair exposed a real second defect: the receiving leg requested only vertical rise, although its compression row works along COM-to-material-foot distance. Forward motion made that row slack. The requested extension now uses the distance change toward the already validated receiving/onward goal, with the original limits. Completion requires positive measured hand and foot load in that same solved event, fresh receiver contact and firm footing. No active-constraint count substitutes for load.

## Executed acceptance

Host Release C++17 native commands and observed output are retained in [receiving-handoff-window.json](receiving-handoff-window.json), including the base revision, changed-source hashes and exact compile/archive/link/run argument arrays. The native archive is a scoped variant of the coherent published76-TU archive; the matching GitHub development candidate separately rebuilds the committed source.

- `physical_topout_radial onward-run`: exit0, `PASS PHYSICAL_TOPOUT onward_run real_receiver=1 forced_stop=0`, release143 at2.26725m/s inward,0.713114m onward travel.
- `physical_topout_radial blocked`: exit0, `PASS PHYSICAL_TOPOUT blocked_receiver no_legacy_action_started=1`.
- `receiver_handoff_radial onward`: exit0, actual receiver1960, completion693/shared-event693, hands428.115N, foot98.4029N, source contact692; ordinary steering reaches the requested shelf point and firm checkpoint footing at tick835.
- `receiver_handoff_radial release`: exit0; deliberate release during the receiving approach detaches hands/plan, with no delayed completion or automatic reacquisition for90native ticks. Real deck contact remains legal.

The public tests retain gravity1, zero deaths, per-hand1500N, foot3500N/0.3m/1285.625J bounds and the existing hand command/work-rate assertions. Tests do not write body motion after initial staging. Existing enum values remain unchanged; ReceiverApproach is appended as9. Existing first-person arms, camera settling and contact sound consume the same native hands and foot-transfer receipt.

## Limits

This is the fixed receiving exit and nearby top-out/release scope. It does not complete the entire ladder/swing campaign or prove Android device feel, FPS or full-project release gates. The earlier zero-load failures are retained as failed evidence, not relabeled as passing.
