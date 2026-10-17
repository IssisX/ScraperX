extends "res://presentation/main_machines.gd"

# The main scene, layer 4 of 6: reading the native. The per-frame context
# (what the player stands on, holds and is offered) and the names of what
# can be carried or rigged.

# Chute is offered once a fall is unmistakably a fall: a full-height jump
# lands at ~5.5 m/s, so 6.5 m/s never pops the canopy button mid-hop.
const CHUTE_OFFER_FALL_MPS := 6.5

# AS-006: the mechanism kit's entity ids (sim/mechanism_kit.hpp): band
# structure from 1000, bodies that move from 2000. What a kit body is, how
# big, what it is made of and where it is all come from the native.
const KIT_ENTITY_MIN := 1000
const KIT_ENTITY_MAX := 3000
const KIT_CARRY_SHACKLE := 1
const KIT_CARRY_HANDLE := 2
# The owner's vertical machines (src/sim/vertical/vertical_route.cpp), in
# route order, and what the player stands on of one (LiftSnapshot.role).
const LIFT_NAMES := ["CASCADE MAST", "PITMAN LIFT", "BARREL HELIX", "CROWN GONDOLA", "LUFFING DERRICK", "SLAB INCLINE", "STONE WHEEL", "TRACTION TRAM"]
const LIFT_ROLE_DECK := 1
const LIFT_ROLE_ENTRY := 2
const LIFT_ROLE_EXIT := 3
# The slab incline is worked from its deck by the rider's weight on its plates.
const LIFT_SLAB_INCLINE := 5
# The traction tram shuttles on its own: Action only reads out its wait.
const LIFT_TRAM := 7
# The luffing derrick is worked by the load on its cradle: Action reads it
# out, and from the lower receiver calls the cradle down.
const LIFT_DERRICK := 4
const INCLINE_ARMED := 0
const INCLINE_RELEASED := 1
const RIG_HOOK := 1
const RIG_UNHOOK := 2
# S1's cage and the scale plate in its floor (Simulation::kStackS1CageEntityId,
# kStackS1PlateEntityId).
const STACK_S1_CAGE := 2200
const STACK_S1_PLATE := 2203

const TRAVERSAL_NONE := 0
const TRAVERSAL_HANGING := 1
# Step 2 movement: on the holds of a ladder, pipe, bar or lattice; and
# lowering over an edge into a hang.
const TRAVERSAL_CLIMBING := 4
# Parkour step 2: hanging from a bar or pipe overhead, swinging hand over hand.
const TRAVERSAL_SWINGING := 6
# The crouch toggle (C, right-stick click, the touch button). Held Ctrl
# crouches too, for as long as it is held; the native decides whether the
# body can crouch or stand.
var _crouch_toggled := false
# The legs drawn up in this flight (the native's tuck): the toggle that did it
# lets go when the feet come down.
var _tucked_this_flight := false
var _trapeze_was_hanging := false

func _read_context() -> Dictionary:
	var landing_state: Dictionary = _native.get_landing_state()
	var position: Vector3 = _native.get_player_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var traversal := int(_native.get_traversal_state())
	# A springboard's plant runs a vault's path (sim kSpringboardMinimumSpeed),
	# but it is a foot on a top, not hands on a rail: the hands, the view and
	# the sounds take it as the run's.
	var springboarding := bool(_native.is_player_springboarding())
	if springboarding:
		traversal = TRAVERSAL_NONE
	var chute := bool(_native.is_parachute_deployed())
	var ledge := bool(_native.is_ledge_available())
	var lethal := float(_native.get_lethal_impact_speed_mps())
	var hanging := traversal == TRAVERSAL_HANGING
	var climbing := traversal == TRAVERSAL_CLIMBING
	var swinging := traversal == TRAVERSAL_SWINGING
	var free := traversal == TRAVERSAL_NONE
	var grip := bool(_native.is_grip_available())
	var edge_drop := bool(_native.is_edge_drop_available())
	var climb_ok := grounded and free and ledge
	# What is on the carry point, and what a pick-up would take -- both the
	# native's own reading, never guessed here.
	var carrying := int(_native.get_carrying_entity_id())
	var carry_target := int(_native.get_carry_target_entity_id())
	# AS-006: the native's own rig reading -- hook what is carried onto the
	# anchor in reach, or take a slack hooked end off -- and what kind of
	# thing a pick-up would take.
	var rig := int(_native.get_rig_action())
	var rig_target := int(_native.get_rig_target_entity_id())
	var sling: Dictionary = _native.get_slingshot_state()
	var sling_seated := bool(sling["seated"])
	var swing: Dictionary = _native.get_swing_state()
	var swing_seated := bool(swing["seated"])
	var zip: Dictionary = _native.get_zipline_state()
	var zip_riding := bool(zip["riding"])
	var trapeze: Dictionary = _native.get_trapeze_state()
	var hook: Dictionary = _native.get_hook_state()
	var trapeze_hanging := bool(trapeze["harnessed"])
	# Off the trapeze, the arms' pull (the crouch toggle while hanging) ends.
	if _trapeze_was_hanging and not trapeze_hanging:
		_crouch_toggled = false
	_trapeze_was_hanging = trapeze_hanging
	if bool(_native.is_player_tucked()):
		_tucked_this_flight = true
	elif grounded and _tucked_this_flight:
		_tucked_this_flight = false
		_crouch_toggled = false
	var lift: Dictionary = _native.get_lift_state()
	var lift_machine := int(lift["machine"])
	var support := int(_native.get_support_entity_id())
	var forklift: Dictionary = _native.get_forklift_state()
	var forklift_driving := bool(forklift["driving"])
	var action := {"id": &"", "label": "", "icon": &"climb", "detail": ""}
	if forklift_driving:
		# At the forklift's wheel: the stick drives and steers, Jump and
		# Crouch held raise and lower the forks, Action lets go.
		action = {"id": &"drive", "icon": &"down", "label": "STEP OFF",
			"detail": "FORKS %.1f M" % float(forklift["lift_m"])}
	elif bool(forklift["at_controls"]) and carrying == 0:
		action = {"id": &"drive", "icon": &"up", "label": "DRIVE", "detail": "THE FORKLIFT"}
	elif bool(sling["can_retrieve"]) or bool(sling["recovering"]):
		action = {"id": &"slingshot", "icon": &"up",
			"label": "STOP REELING" if bool(sling["recovering"]) else "RETRIEVE POUCH",
			"detail": "HOLD BACK TO REEL IT IN" if bool(sling["recovering"]) else "REEL THE SPENT POUCH BACK"}
	elif (sling_seated or bool(sling["station_available"])) and not bool(sling["released"]):
		var drawn := "%.1f M  %d KJ" % [float(sling["draw_m"]), roundi(float(sling["energy_j"]) / 1000.0)]
		if not sling_seated:
			action = {"id": &"slingshot", "icon": &"up", "label": "ENTER POUCH", "detail": "SLINGSHOT"}
		elif bool(sling["release_ready"]):
			action = {"id": &"slingshot", "icon": &"up", "label": "RELEASE", "detail": drawn}
		else:
			action = {"id": &"slingshot", "icon": &"up", "label": "PULL BACK",
				"detail": "STICK BACK TO DRAW  " + drawn}
	elif bool(hook["take_available"]):
		action = {"id": &"hook_take", "icon": &"pick_up", "label": "TAKE HOOK", "detail": "A GRAPPLING HOOK"}
	elif trapeze_hanging:
		# On the trapeze: Crouch pulls up on the bar, Action lets go.
		action = {"id": &"trapeze", "icon": &"down", "label": "LET GO",
			"detail": "CROUCH: PULL UP" if not bool(trapeze["pulled"]) else "CROUCH: LET DOWN"}
	elif bool(trapeze["catch_available"]) or bool(trapeze["catch_near"]):
		action = {"id": &"trapeze", "icon": &"up", "label": "GRAB THE BAR", "detail": "THE TRAPEZE"}
	elif swing_seated:
		# Strapped into the swing: kick the trip while the ram is on its hook;
		# held at the top by the rack, unbuckle and step off.
		if bool(swing["ram_held"]):
			action = {"id": &"swing", "icon": &"up", "label": "KICK THE TRIP", "detail": "LET THE RAM GO"}
		elif bool(swing["held_at_top"]) and float(swing["seat_speed_mps"]) < 0.3:
			action = {"id": &"swing", "icon": &"up", "label": "UNBUCKLE",
				"detail": "STEP OFF  %.0f M" % float(swing["seat_floor_y"])}
	elif bool(swing["station_available"]):
		action = {"id": &"swing", "icon": &"up", "label": "STRAP IN", "detail": "THE SWING"}
	elif grounded and free and carrying == 0 and lift_machine >= 0 and \
			not (lift_machine == LIFT_DERRICK and carry_target != 0):
		# One of the owner's vertical machines, on its own motor: from its deck
		# Action sends the deck to the other end; from a receiver it calls it.
		var lift_name: String = LIFT_NAMES[lift_machine] if lift_machine < LIFT_NAMES.size() else "LIFT"
		var role := int(lift["role"])
		var lift_travel := float(lift["travel"])
		if lift_machine == LIFT_DERRICK:
			var iron := int(lift["derrick_iron_aboard"])
			if role == LIFT_ROLE_DECK and lift_travel < 0.01 and not bool(lift["moving"]):
				action = {"id": &"derrick", "icon": &"up", "label": "TOO HEAVY",
					"detail": "IRON ABOARD: %d" % iron}
			elif role == LIFT_ROLE_DECK and lift_travel < 0.99:
				action = {"id": &"derrick", "icon": &"up", "label": "RISING", "detail": "IRON ABOARD: %d" % iron}
			elif role == LIFT_ROLE_ENTRY and lift_travel > 0.01 and not bool(lift["derrick_holding"]):
				action = {"id": &"lift", "icon": &"up", "label": "CALL", "detail": lift_name}
		elif lift_machine == LIFT_TRAM:
			# Nothing to press: it leaves each end when its wait is up.
			var departs := float(lift["tram_departs_s"])
			var here_end := 1.0 if role == LIFT_ROLE_EXIT else 0.0
			if departs < 0.0:
				action = {"id": &"tram", "icon": &"up", "label": "RIDING" if role == LIFT_ROLE_DECK else "WAIT",
					"detail": "THE TRAM" if role == LIFT_ROLE_DECK else "THE TRAM IS ON ITS WAY"}
			elif role == LIFT_ROLE_DECK:
				action = {"id": &"tram", "icon": &"up", "label": "STEP OFF" if lift_travel > 0.5 else "STAY ON",
					"detail": "LEAVES IN %d S" % ceili(departs)}
			elif absf(lift_travel - here_end) < 0.02:
				action = {"id": &"tram", "icon": &"up", "label": "BOARD", "detail": "LEAVES IN %d S" % ceili(departs)}
			else:
				action = {"id": &"tram", "icon": &"up", "label": "WAIT", "detail": "THE TRAM IS AT THE OTHER END"}
		elif role == LIFT_ROLE_DECK and lift_machine == LIFT_SLAB_INCLINE:
			# Nothing to press: the yellow plate pulls the slab's pin, the
			# treadle presses the brake shoe. Action reads out which.
			var incline_state := int(lift["incline_state"])
			if incline_state == INCLINE_ARMED and lift_travel < 0.01:
				action = {"id": &"incline", "icon": &"up", "label": "STAND ON", "detail": "THE YELLOW PLATE"}
			elif incline_state == INCLINE_RELEASED and lift_travel < 0.99:
				action = {"id": &"incline", "icon": &"down",
					"label": "BRAKING" if float(lift["incline_brake_n"]) > 1000.0 else "BRAKE: THE TREADLE",
					"detail": "%.1f M/S" % float(lift["incline_speed_mps"])}
		elif lift_machine == int(lift["pitman"]):
			# Worked by the hands: ACTION held hauls its line. On the deck a
			# gun tackle lifts the rider; from the lower receiver the call
			# line hauls the deck down; from the upper one the hands free its
			# brake and the empty deck rises on its counterweight.
			var pull := roundi(float(lift["pitman_haul_n"]))
			if role == LIFT_ROLE_DECK and lift_travel < 0.995:
				action = {"id": &"haul", "icon": &"up", "label": "HAUL",
					"detail": ("%d N" % pull) if pull > 0 else "HOLD TO HAUL THE LINE"}
			elif role == LIFT_ROLE_ENTRY and lift_travel > 0.01:
				action = {"id": &"haul", "icon": &"down", "label": "HAUL DOWN", "detail": "THE CALL LINE"}
			elif role == LIFT_ROLE_EXIT and lift_travel < 0.99:
				action = {"id": &"haul", "icon": &"up", "label": "FREE BRAKE", "detail": "IT RISES EMPTY"}
		elif role == LIFT_ROLE_DECK:
			if float(lift["target"]) < 0.5:
				action = {"id": &"lift", "icon": &"up", "label": "RAISE", "detail": lift_name}
			else:
				action = {"id": &"lift", "icon": &"up", "label": "LOWER", "detail": lift_name}
		elif (role == LIFT_ROLE_ENTRY and lift_travel > 0.01) or (role == LIFT_ROLE_EXIT and lift_travel < 0.99):
			action = {"id": &"lift", "icon": &"up", "label": "CALL", "detail": lift_name}
	elif grounded and free and carrying == 0 and (support == STACK_S1_CAGE or support == STACK_S1_PLATE) and \
			float(_native.get_stack_s1_cage_travel()) < 0.05:
		# In S1's cage at grade there is nothing to press: the rider's weight
		# on the plate works the valve. Action only reads out what it does.
		if float(_native.get_stack_s1_valve_angle()) > 0.06:
			action = {"id": &"s1", "icon": &"up", "label": "FILLING",
				"detail": "BUCKET %d KG" % roundi(float(_native.get_stack_s1_bucket_water_kg()))}
		else:
			action = {"id": &"s1", "icon": &"up", "label": "STAND ON", "detail": "THE SCALE PLATE"}
	elif hanging or climbing:
		action = {"id": &"climb_up", "label": "CLIMB UP", "icon": &"climb", "detail": ""}
	elif swinging and zip_riding:
		# On a zipline's trolley: the line does the moving; JUMP OFF throws
		# the body on, DROP lets go.
		action = {"id": &"let_go", "label": "LET GO", "icon": &"down",
			"detail": "ZIPLINE  %d M/S" % roundi(absf(float(zip["speed_mps"])))}
	elif swinging:
		# On a bar overhead: forward pumps the swing and reaches for the next
		# bar; letting go flies on with the swing.
		action = {"id": &"let_go", "label": "LET GO", "icon": &"down", "detail": "FORWARD: SWING ON"}
	elif not free:
		pass
	elif carrying != 0 and rig == RIG_HOOK:
		action = {"id": &"hook", "label": "HOOK", "icon": &"hook",
			"detail": "ONTO " + _kit_anchor_name(rig_target)}
	elif carrying != 0:
		# Both hands are on it: letting go is the only thing Action can do.
		action = {"id": &"set_down", "label": "LET GO", "icon": &"set_down", "detail": _carry_name(carrying)}
	elif grounded and carry_target != 0:
		# Ahead of CLIMB: whatever a load rests on may itself be a mantle
		# ledge, and whoever faces the load means the load.
		var kind := int(_native.get_carry_target_kind())
		var verb := "GRAB" if kind == KIT_CARRY_HANDLE else ("TAKE" if kind == KIT_CARRY_SHACKLE else "PICK UP")
		action = {"id": &"pick_up", "label": verb, "icon": &"pick_up",
			"detail": _carry_name(carry_target)}
	elif grounded and rig == RIG_UNHOOK:
		action = {"id": &"unhook", "label": "UNHOOK", "icon": &"hook", "detail": "ROPE END"}
	elif climb_ok:
		action = {"id": &"climb", "label": "CLIMB", "icon": &"climb",
			"detail": "%+.1f M" % float(_native.get_ledge_rise_meters())}
	elif grounded and free and grip:
		# A hold at hand height: a rung, a pipe, a scaffold bar.
		action = {"id": &"climb", "label": "CLIMB", "icon": &"climb", "detail": "HOLD"}
	return {
		"position": position,
		"velocity": velocity,
		"grounded": grounded,
		"crouched": bool(_native.is_player_crouched()),
		"sliding": bool(_native.is_player_sliding()),
		"wall_running": bool(_native.is_player_wall_running()),
		"wall_climbing": bool(_native.is_player_wall_climbing()),
		"wall_steps": int(_native.get_wall_step_count()),
		"springboarding": springboarding,
		"springboards": int(_native.get_springboard_count()),
		"traversal": traversal,
		"hanging": hanging,
		"climbing": climbing,
		"swinging": swinging,
		"zipline_riding": zip_riding,
		# Drop at an edge behind lowers into a hang; on holds it lets go; in
		# the slingshot's pouch it climbs out.
		"has_hook": bool(hook["has_hook"]),
		"hook_state": int(hook["state"]),
		"drop_ok": int(hook["state"]) != 0 or hanging or climbing or swinging or \
			(grounded and free and edge_drop and carrying == 0 and not forklift_driving) or \
			sling_seated or bool(sling["recovering"]) or (swing_seated and bool(swing["may_leave"])) or \
			trapeze_hanging,
		"sling_seated": sling_seated,
		"swing_seated": swing_seated,
		"trapeze_hanging": trapeze_hanging,
		"forklift_driving": forklift_driving,
		"trapeze_pulled": bool(trapeze["pulled"]),
		"sling_recovering": bool(sling["recovering"]),
		"sling_flight": bool(sling["flight"]),
		"edge_drop": edge_drop,
		"sprinting": bool(_native.is_player_sprinting()),
		"balancing": bool(_native.is_player_balancing()),
		"landing": landing_state,
		"chute": chute,
		"jump_ok": grounded and free,
		# The precision jump's sighted landing (PARKOUR_MOVE_SET.md M3).
		"precision": grounded and free and bool(_native.is_precision_sighted()),
		"precision_point": _native.get_precision_point(),
		"climb_ok": climb_ok,
		# Airborne, is_ledge_available is the native hang probe: an edge in
		# the grab band, which engages as soon as the player pushes into it.
		"grab_hint": not grounded and free and ledge,
		# Thrown off the slingshot, the chute is offered from the top of the
		# throw: that is where a rider steers for a landing.
		"chute_ok": not grounded and free and (chute or -velocity.y > CHUTE_OFFER_FALL_MPS or
			(bool(sling["flight"]) and velocity.y < 0.0)),
		"danger": 0.0 if grounded else clampf(-velocity.y / maxf(lethal, 0.001), 0.0, 1.0),
		"lethal": lethal,
		"carrying": carrying,
		"action": action,
		"checkpoint": _native.get_checkpoint_position(),
		"deaths": int(_native.get_death_count()),
		"tower_height": float(_native.get_tower_height_meters()),
		# The altimeter's scale: the top of the route as built, not the
		# tower's full mass.
		"route_top": float(landing_state["route_top"]),
	}

func _carry_center(entity: int) -> Vector3:
	if _is_kit(entity):
		var body := int(_native.get_kit_body_index(entity))
		if body >= 0:
			var at: Transform3D = _native.get_kit_body_transform(body)
			# Loose stock is lifted by its top (loose_stock.cpp's handles):
			# a drum by its rim, a pallet by its deck.
			if entity >= STOCK_FIRST_ENTITY and entity < STOCK_ENTITY_LIMIT:
				_stock_name(entity)
				match int(_stock_kinds.get(entity, -1)):
					3:
						return at * Vector3(0.0, 0.44, 0.0)
					4:
						return at * Vector3(0.0, 0.07, 0.0)
			return at.origin
	return Vector3.ZERO

func _carry_half(entity: int) -> float:
	if _is_kit(entity):
		var body := int(_native.get_kit_body_index(entity))
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body) if body >= 0 \
			else PackedFloat32Array()
		if parts.size() >= KIT_PART_FLOATS:
			return clampf(parts[0], 0.08, 0.30)
	return 0.18

func _carry_name(entity: int) -> String:
	match entity:
		2002, 2012:
			return "ROPE SHACKLE"
		2004, 2014:
			return "TRIP HANDLE"
		2006:
			return "TIP-OUT HANDLE"
		2024:
			return "REBAR LINE"
		2026:
			return "LATCH HANDLE"
		2031:
			return "PIPE SPOOL"
		2033:
			return "FILL VALVE"
		2035:
			return "DRAIN HANDLE"
		2042:
			return "DOOR"
		2043:
			return "DOOR LATCH"
		2958, 2959, 2960, 2961, 2962, 2963:
			return "IRON PIG"
		2979:
			return "SKIP LINE"
		2045:
			return "CHILLER TRIP"
		2052:
			return "HOSE"
		2054:
			return "STOP VALVE"
		2061:
			return "HEADER DUMP"
		2072:
			return "PROP PIN"
		2073, 2086, 2095:
			return "LANYARD"
		2074, 2096:
			return "ROPE SHACKLE"
		2084:
			return "TAIL PIN"
		2093:
			return "DOMINO PIN"
		2102:
			return "RAIL JOINT"
		2104, 2113, 2125, 2133:
			return "LANYARD"
		2112:
			return "PENDANT PIN"
		2114, 2134:
			return "ROPE SHACKLE"
		2138:
			return "FLAP CHAIN"
		2146:
			return "CHOCK LANYARD"
		2124:
			return "DROP PIN"
		2127:
			return "CLUTCH HANDLE"
	if entity >= STOCK_FIRST_ENTITY and entity < STOCK_ENTITY_LIMIT:
		return _stock_name(entity)
	return "ROPE END" if _is_kit(entity) else ""

# Loose stock that is carried (src/sim/loose_stock.cpp), by its kind.
var _stock_kinds := {}
func _stock_name(entity: int) -> String:
	if _stock_kinds.is_empty():
		var stock: PackedInt32Array = _native.get_loose_stock()
		var piece := 0
		for i in range(0, stock.size() - 2, 3):
			_stock_kinds[STOCK_FIRST_ENTITY + piece] = stock[i + 1]
			piece += 1
	match int(_stock_kinds.get(entity, -1)):
		3:
			return "OIL DRUM"
		4:
			return "PALLET"
	return ""

func _is_kit(entity: int) -> bool:
	return (entity >= KIT_ENTITY_MIN and entity < KIT_ENTITY_MAX) or \
		(entity >= STOCK_FIRST_ENTITY and entity < STOCK_ENTITY_LIMIT)

func _kit_anchor_name(entity: int) -> String:
	match entity:
		1000:
			return "BOLLARD"
		1001:
			return "CLEAT"
		2000, 2010:
			return "CAGE EYE"
		2050:
			return "RAM INLET"
		2070:
			return "PLATFORM EYE"
		2090, 2110, 2130:
			return "CAGE EYE"
		1007:
			return "CLEAT"
	return "ANCHOR"
