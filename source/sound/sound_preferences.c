/*
SOUND_PREFERENCES.C

symbols in this file:
001BF310 0010:
	_read_sound_preferences (0000)
001BF320 0010:
	_write_sound_preferences (0000)
00317A84 001c:
	_data_00317a84 (0000)
	_sound_channel_type_flags (0014)
*/

/* ---------- headers */

#include "sound_preferences.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

/* ---------- prototypes */

/* ---------- globals */

static struct sound_preferences default_sound_preferences =
{
	0,
	{ 10, 51, 10, 10, 1 },
	{ 9, 46, 9, 9, 1 },
	0,
};

/* port: and one channel of uncompressed stereo at 44 kHz, the format
dsound_initialize_channel gives a channel without the compressed bit, for a
sound made at load (port/linux/game/coop_cryo_line.c) */
short sound_channel_type_flags[5] = { 8, 9, 10, 14, 6 };

/* ---------- public code */

void read_sound_preferences(struct sound_preferences **preferences)
{
	*preferences = &default_sound_preferences;
}

void write_sound_preferences(void)
{
}

/* ---------- private code */
