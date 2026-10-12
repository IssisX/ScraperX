# Gravity-cart miss view intent

Exact86fa732 Actions38292013935 stopped in the native suite at gravity_cart_integration. Its deliberate walking miss did not reach the actual341 m apron: the steering helper turns the player toward the target behind the departing cart, and shared physical catches attach both hands to cart2320. A source-bound current native logger replay reproduces the held Climbing state near352 m. The actual1956 apron exists and contains the unchanged target.

The test now retains its explicit outward(+X) view after steering the world-space movement input toward the apron. This matches the touch control's view-preserving air steering. Production physics, generic catches, movement targets,900-tick deadline, receiver/death checks and all finite machine work/force/power assertions are unchanged.

A single fixed private candidate passes the complete retained test: PASS0/231.798s, log SHA64ceec245bebd5713706db4cb5e8a82dc8dd6b18e708302c9b44e217c8029ce8. Actual1956 recovery reaches341.899994 m without deaths; paid recall, reboarding,352 m retry, upper1957 onward control and full330 m reset pass. The candidate adds logger-only observations as well as the ordinary facing input. Its input logger records the helper's direction before the explicit outward override; those printed values are not the final delivered facing.

Classification: existing test-input assumption exposed by shared catch behavior. Current versus original cached archive source provenance establishes no supported role for the latest north geometry repair; no original-archive runtime is claimed. Full GREEN exact-source Actions and matching APK remain separate delivery gates.
