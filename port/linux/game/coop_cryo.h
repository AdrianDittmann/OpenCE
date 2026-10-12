/*
COOP_CRYO.H

Network co-op on The Pillar of Autumn with a cryotube for every player
(coop_cryo.c).
*/

#ifndef __COOP_CRYO_H
#define __COOP_CRYO_H

struct scenario_object_datum;
struct tag_block;

/* ---------- prototypes */

/* network_coop.c */
void coop_cryo_new_game(
	void);
void coop_cryo_update(
	void);

/* game.c's game_is_cooperative */
boolean coop_cryo_scripts_cooperative(
	void);

/* object_types.c's object_types_place_all: Bungie's static tubes in the bay */
boolean coop_cryo_placement_skipped(
	short object_type,
	struct scenario_object_datum const *placement,
	struct tag_block *palette);

/* object_types.c's object_types_place_objects: the cells, on a10a */
void coop_cryo_place_cells(
	void);
/* cache_files.c's scenario_tags_unload */
void coop_cryo_tags_unloaded(
	void);

/* coop_cryo_cell.c: the cell's scenery tag, made the first time asked on
a map, from the loaded a10a (NONE if it can't be) */
long coop_cryo_cell_definition(
	void);
void coop_cryo_cell_release(
	void);

/* coop_cryo_line.c: Keyes' sound, his line made the first time asked on a
map (NONE if it can't be) */
long coop_cryo_line_definition(
	void);
void coop_cryo_line_release(
	void);

/* hs_runtime.c's hs_evaluate_wake: when a script just woken runs */
long coop_cryo_script_wake_time(
	short script_index,
	long sleep_until);

/* players.c: a player held back from spawning whose tube waits */
boolean coop_cryo_seat_waiting(
	long player_index);
void coop_cryo_seat_player(
	long player_index);

/* units.c's unit_exit_seat_end, before the biped stands */
void coop_cryo_exit_seat_end(
	long unit_index,
	long vehicle_index);

#endif
