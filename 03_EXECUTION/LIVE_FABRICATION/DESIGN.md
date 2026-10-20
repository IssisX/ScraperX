# Live fabrication: objects made to order while you play

The owner, 2026-10-20: a 3D object generated while the game is played, *"integrate that in smoothly
and canonically"*. Models run on a server, never on the phone (directives). This is the design;
nothing is built yet. Facts not yet measured are marked **to verify**.

## For the owner, in plain words

The tower has a **Stores**: a requisition counter with a pneumatic dispatch hatch, the same spares
system the glass tube belongs to. You tap the counter and write what you need on a requisition slip:
"a crowbar", "a heavy weight", "a steel bucket", "something to throw". The slip goes down the line;
somewhere below, the night shift's fabrication shop makes it. You hear the shop working through the
pipes. Half a minute or so later the hatch thumps, the carrier hisses open, and there it is: a real,
physical object, made for you, with real weight. You can carry it, throw it at her, drop it on a
scale plate, wedge a door with it.

Everything it is comes from physics, like the rest of the game: a steel weight is heavy because it
is steel and that big; a timber plank floats; a bucket holds what you pour in. What you can order is
bounded by the canon: it must fit the dispatch carrier (about 1.2 m), and you must be able to carry
it. The route never needs one: with no signal, the Stores is simply closed for the night.

The horror layer, later: sometimes the shop sends back something you did not order. And once the
badge photo exists (the owner's idea; it needs the camera permission, the owner's call), the shop
has your face on file.

## How it fits the world

- **The Stores and its spares lines** are the canon: a stores counter at grade in the yard (so it
  works from the start), and the glass spares tube's stations as the tower's upper deliveries. The
  requisition slip, the dispatch carrier, the hatch and its whoosh are the in-world interface; no
  menu.
- **The wait is diegetic.** A generation takes tens of seconds (**to verify**); the slip's status
  shows on the counter's printer ("RECEIVED", "IN FABRICATION", "DISPATCHED"), the shop is heard
  through the pipes, the carrier arrives when the object is ready.
- **Offline is canon.** No connection, a quota spent, a refused request: the counter's lamp is out
  or the slip comes back stamped REFUSED. Nothing on the route depends on the Stores.

## How it works

1. **The slip** (the game): a category (tool, weight, container, junk, part) and free text, a
   material (steel, timber, rubber, concrete) and a size class. Sent to the game's own server with a
   per-install id and a nonce. The phone never holds a model provider's key.
2. **The server** (ours, small): rate limit and daily quota, text moderation, a cache keyed by the
   normalized slip (a crowbar already made is served at once and free), then the generator (a hosted
   text-to-3D service, or an open model on a rented GPU: **to verify** licenses, cost and time per
   object before choosing).
3. **Made fit for the game** (server, every object):
   - scaled to the requested size and to the carrier's bound;
   - triangles cut to a phone budget (about 4,000) and one texture of 512 px;
   - split into at most 8 convex hulls of at most 32 points each, for the physics;
   - volume and mass from the hulls and the material's density;
   - checked (finite numbers, sane bounds) and packed as a GLB plus a hull file, then **signed**.
4. **Delivered** (the game): downloads at most 2 MB, checks the signature and every bound before
   parsing anything, caches it by its hash, and hands the hulls to the native simulation, which
   builds the body; the presentation loads the GLB (the game already loads GLBs at runtime: the
   climber's body, the Matron). It arrives in the dispatch carrier.

## The rules that keep it honest

- **The native simulation owns it.** The object is a real Jolt body built from its hulls, with the
  mass its material gives. Nothing about it is scripted.
- **Determinism (plan rule 10).** Generation happens outside the simulation; its result is an input
  at the boundary, like a touch. The object's hull data and hash go into the save, so the same save
  and the same inputs give the same end, bit for bit.
- **Body order.** Fabricated objects take entity ids from a range of their own, created after
  everything else. The body-order fix keys the solver's contact order on entity ids, so a delivery
  leaves every other band's simulation unchanged; the native falsifier "body order" is extended to
  prove it. The range must sit above every existing range, including the corpse check's wide window
  (`physics_world_support.hpp`, ids 3300 to 3300 + 65536): from 0x20000 up (**to verify** nothing
  else uses it).
- **Bounds on everything that comes in** (it is untrusted data): file size, triangle and texture
  limits, hull and point counts, finite coordinates, mass and size ranges, and the signature. A
  delivery that fails any check is refused, never repaired on the phone.
- **No bypass.** A fabricated object is loose stock: it cannot be fixed to the structure, and its
  size bound (the carrier) keeps it from bridging the gaps the climbs are built around. Machines may
  be fed with it (a weight on a plate is the physics working).
- **Saves.** A save holds each fabricated object's hull data and hash; on load, a missing GLB is
  fetched again by hash or drawn as a plain crate of the same hulls; the physics never changes.

## Safety, privacy, cost

- **Text** is moderated on the server; refused slips come back stamped.
- **Photos** (badge photo, later): processed on the server and deleted; never stored, never sent to
  the generator as a face record; a consent screen first. Face data is regulated in some places
  (**to verify** the rules that apply before building it).
- **Cost** is capped on the server: a daily quota per install and a global daily budget; the cache
  makes common requests free.
- **Store policy:** app stores have rules for AI-generated content, including in-app reporting
  (**to verify** before release). A "report this object" on each delivery is planned.
- **Permissions:** the live version needs the INTERNET permission in the APK; the badge photo needs
  CAMERA. Both are the owner's to approve (the standing rule on permissions).

## Build order, each step proven before the next

1. **The object path, offline.** A tool (`tools/fab/`) turns any GLB into the game's format
   (scale, cut, hulls, mass, sign). The native simulation builds a body from it; a native falsifier
   proves the same data gives the same result and that adding one leaves other bands bit-identical.
   A few objects are made while we build (Hugging Face models, licenses checked) and tried in a test
   world. No server, no new permission.
2. **The Stores in the game,** working from a library of objects made in step 1: the counter, the
   slip, the wait, the carrier and the hatch, the object in the world, saved and loaded. Played by
   touch. Still no server.
3. **The server and the live slip,** behind a setting that is off until the owner approves the
   INTERNET permission: cache, quota, moderation, generation, signing; then measured cost and time
   per object.
4. **The badge photo,** when the owner approves CAMERA: the face on file.
5. **The wrong deliveries:** the shop sends back what you did not order.

## Decisions for the owner

1. The canon: the Stores and its spares lines (recommended), or another.
2. What a slip can say: a category plus free text (recommended), or a fixed catalog.
3. The generator: a hosted service to start (fastest to build), or an open model on our own GPU
   (cheaper at scale, more work). Recommended: hosted for step 3, decided again on measured cost.
4. The permissions: INTERNET for step 3, CAMERA for step 4.
5. A daily budget for the server.

## The horror layer (the owner, 2026-10-20: *"How do you think we could use that to creep the player out even more? Not a gimmick bolted on"*)

The Stores becomes hers. Personal, because the generator answers what you asked; slow, because it
escalates. Each idea rests on something the game already has.

1. **She reads your slips** (the Stores): orders arrive right, then subtly wrong (the crowbar bent
   into a hook, the bucket half full of something dark), then as she wants them.
2. **The printer at night** (her hoard, her mischief): slips you did not write print in her hand
   ("1 NURSE'S CAP", "1 HAND, LEFT, SIZE 7"); the objects turn up in her hoard later.
3. **Return to sender** (the throw, her return, torn limbs): what you throw at her comes back
   through the Stores mended and marked; once limbs tear, the tube delivers her arm, stamped.
4. **The dead crew's last orders** (the overtime crew she killed): their unfilled slips arrive
   decades late, one object each; the story fixed, the objects different every playthrough.
5. **Your face on file** (the badge photo, CAMERA): a sheet-metal mask of your face in her locker,
   a missing-worker poster, then a body in your clothes with your face at the shaft's foot.
6. **Heavier than it looks** (the physics): a crate far heavier than its material allows; you feel
   it in the carry; dropped from height, it splits.

Claude's picks to start: 1 and 3 (no new permission), then 5 when CAMERA is approved.
