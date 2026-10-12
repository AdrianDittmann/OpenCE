/*
SOUND_PREFERENCES.H

header included in hcex build.
*/

#ifndef __SOUND_PREFERENCES_H
#define __SOUND_PREFERENCES_H
#pragma once

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

/* port: five channel types, one more than the Xbox's (sound_preferences.c) */
struct sound_preferences
{
	short platform;
	short actual_channel_counts[5];
	short virtual_channel_counts[5];
	short unused;
};

/* ---------- prototypes/SOUND_PREFERENCES.C */

void read_sound_preferences(
	struct sound_preferences **preferences);
void write_sound_preferences(
	void);

/* ---------- globals */

extern short sound_channel_type_flags[5];

/* ---------- public code */

#endif // __SOUND_PREFERENCES_H
