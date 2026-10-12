/*
COOP_CRYO_LINE.C

Keyes' line for network co-op's cryo bay (coop_cryo.c), "Bridge to Cryo One
through Sixteen": the loaded a10's own A10_350 ("Bridge to Cryo Two") with
the words of port/assets/sounds/cryo_one_through_sixteen.wav in place of
its "Cryo Two", with less of the map's 4-bit noise, played over the map's
uncompressed (loose_sounds_replace).
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/errors.h"
#include "cache/cache_files.h"
#include "sound/sound_definitions.h"
#include "tag_files/tag_groups.h"

#include "coop_cryo.h"
#include "custom_edition_cache.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* tools/embed_assets.py's */
extern unsigned int const coop_cryo_words[];
extern unsigned long const coop_cryo_words_size;

boolean loose_sounds_replace(long tag_index, byte *samples, unsigned long size, short compression, short encoding,
	short sample_rate, long length);

/* ---------- constants */

#define CRYO_LINE_SOUND "sound\\dialog\\a10\\a10_350_captkeyes"

enum
{
	CRYO_LINE_RATE = 22050,
	CRYO_LINE_RECORDING_FRAMES = 98880,
	CRYO_LINE_WORDS_FRAMES = 39031,
	CRYO_LINE_CROSSFADE = 66,
	CRYO_LINE_MAXIMUM_PERMUTATIONS = 8,
	CRYO_LINE_SECTOR_SIZE = 512,
	CRYO_LINE_HISS_WINDOW = 220,
	CRYO_LINE_GAIN_WINDOW = 110,

	CRYO_COMPRESSION_NONE = 0,
	CRYO_COMPRESSION_XBOX_ADPCM = 1,
	CRYO_ENCODING_STEREO = 1,
	CRYO_SAMPLE_RATE_44K = 1,
};

/* the recording's frames and the words', in order; the fourth, the /s/ of
its "send", makes "Master Chiefs" */
static struct cryo_line_run
{
	boolean words;
	long start;
	long end;
} const cryo_line_runs[] =
{
	{ FALSE, 0, 12502 }, { TRUE, 0, 39031 }, { FALSE, 26438, 73710 }, { FALSE, 50006, 51973 }, { FALSE, 73644, 98880 },
};

/* ---------- structures */

struct cryo_biquad
{
	double b0;
	double b1;
	double b2;
	double a1;
	double a2;
	double x1;
	double x2;
	double y1;
	double y2;
};

/* ---------- globals */

static struct
{
	boolean tried;
	long definition_index;
} coop_cryo_line = { FALSE, NONE };

/* ---------- private code */

static unsigned long cryo_line_read_le(
	byte const *bytes,
	short count)
{
	unsigned long value = 0;

	while (count-- > 0)
		value = (value << 8) | bytes[count];
	return value;
}

static byte const *cryo_line_words(
	void)
{
	byte const *file = (byte const *)coop_cryo_words;
	unsigned long size = coop_cryo_words_size;
	unsigned long offset = 12;
	boolean format = FALSE;

	if (size < 12 || memcmp(file, "RIFF", 4) || memcmp(file + 8, "WAVE", 4))
		return NULL;
	while (size - offset >= 8)
	{
		byte const *chunk = file + offset + 8;
		unsigned long chunk_size = cryo_line_read_le(file + offset + 4, 4);

		if (chunk_size > size - offset - 8)
			return NULL;
		if (!memcmp(file + offset, "fmt ", 4))
		{
			format = chunk_size >= 16 && cryo_line_read_le(chunk, 2) == 1 && cryo_line_read_le(chunk + 2, 2) == 1 &&
				cryo_line_read_le(chunk + 4, 4) == CRYO_LINE_RATE && cryo_line_read_le(chunk + 14, 2) == 16;
		}
		else if (!memcmp(file + offset, "data", 4))
		{
			return format && chunk_size == 2 * CRYO_LINE_WORDS_FRAMES ? chunk : NULL;
		}
		offset += 8 + chunk_size + (chunk_size & 1);
		if (offset > size)
			return NULL;
	}
	return NULL;
}

/* (a buffer of the game's allocator) */
static short *cryo_line_recording(
	struct sound_definition const *definition)
{
	struct sound_pitch_range const *range;
	short *frames;
	long frame_count = 0;
	short permutation_index = 0;
	short step;

	if (definition->compression != CRYO_COMPRESSION_XBOX_ADPCM || definition->encoding != 0 ||
		definition->sample_rate != 0 || definition->pitch_ranges.count < 1 ||
		!(frames = malloc(CRYO_LINE_RECORDING_FRAMES * sizeof(*frames))))
	{
		return NULL;
	}
	range = TAG_BLOCK_GET_ELEMENT(&definition->pitch_ranges, 0, struct sound_pitch_range);
	for (step = 0; permutation_index != NONE && step < CRYO_LINE_MAXIMUM_PERMUTATIONS; step++)
	{
		struct sound_permutation const *permutation;
		byte *samples;
		short *decoded = NULL;
		long decoded_count = 0;
		boolean read = FALSE;

		if (!VALID_INDEX(permutation_index, range->permutations.count))
			break;
		permutation = TAG_BLOCK_GET_ELEMENT(&range->permutations, permutation_index, struct sound_permutation);
		if (permutation->samples.size <= 0 || permutation->samples.size > MAXIMUM_SOUND_DATA_SIZE)
			break;
		samples = malloc((permutation->samples.size + CRYO_LINE_SECTOR_SIZE - 1) & ~(CRYO_LINE_SECTOR_SIZE - 1));
		if (!samples)
			break;
		/* (by its cache tag index, as xbox_sound_cache.c reads it) */
		cache_file_read(permutation->unknown2, permutation->samples.file_offset, permutation->samples.size, samples,
			&read, TRUE);
		cache_file_block_until_not_busy();
		if (read == TRUE)
		{
			decoded = custom_edition_sounds_decode_frames(samples, permutation->samples.size,
				CRYO_COMPRESSION_XBOX_ADPCM, 1, CRYO_LINE_RATE, &decoded_count);
		}
		free(samples);
		if (!decoded || decoded_count > CRYO_LINE_RECORDING_FRAMES - frame_count)
		{
			if (decoded)
				free(decoded);
			break;
		}
		memcpy(frames + frame_count, decoded, decoded_count * sizeof(*frames));
		frame_count += decoded_count;
		free(decoded);
		permutation_index = TEST_FLAG(definition->flags, _sound_definition_linked_permutations_bit) ?
			permutation->next_permutation_index : NONE;
	}
	if (permutation_index != NONE || frame_count != CRYO_LINE_RECORDING_FRAMES)
	{
		free(frames);
		return NULL;
	}
	return frames;
}

static long cryo_line_splice(
	short const *recording,
	byte const *words,
	short *line)
{
	long count = 0;
	short run_index;

	for (run_index = 0; run_index < NUMBEROF(cryo_line_runs); run_index++)
	{
		struct cryo_line_run const *run = &cryo_line_runs[run_index];
		long frame;

		for (frame = run->start; frame < run->end; frame++)
		{
			long sample = run->words ? (short)cryo_line_read_le(words + 2 * frame, 2) : recording[frame];
			long fade = frame - run->start;

			if (run_index > 0 && fade < CRYO_LINE_CROSSFADE)
			{
				short *blended = &line[count - CRYO_LINE_CROSSFADE + fade];

				*blended = (short)(*blended + (sample - *blended) * fade / (CRYO_LINE_CROSSFADE - 1));
			}
			else
			{
				line[count++] = (short)sample;
			}
		}
	}
	return count;
}

/* a Butterworth filter of 2 * `count` poles */
static void cryo_line_filter_new(
	struct cryo_biquad *sections,
	short count,
	boolean high,
	double frequency,
	double rate)
{
	double w0 = 2.0 * M_PI * frequency / rate;
	short index;

	memset(sections, 0, count * sizeof(*sections));
	for (index = 0; index < count; index++)
	{
		struct cryo_biquad *section = &sections[index];
		double alpha = sin(w0) * cos((2 * index + 1) * M_PI / (4.0 * count));
		double c = cos(w0);

		section->b0 = (high ? 1.0 + c : 1.0 - c) / 2.0 / (1.0 + alpha);
		section->b1 = (high ? -(1.0 + c) : 1.0 - c) / (1.0 + alpha);
		section->b2 = section->b0;
		section->a1 = -2.0 * c / (1.0 + alpha);
		section->a2 = (1.0 - alpha) / (1.0 + alpha);
	}
}

static double cryo_line_filter_run(
	struct cryo_biquad *sections,
	short count,
	double sample)
{
	short index;

	for (index = 0; index < count; index++)
	{
		struct cryo_biquad *section = &sections[index];
		double out = section->b0 * sample + section->b1 * section->x1 + section->b2 * section->x2 -
			section->a1 * section->y1 - section->a2 * section->y2;

		section->x2 = section->x1;
		section->x1 = sample;
		section->y2 = section->y1;
		section->y1 = out;
		sample = out;
	}
	return sample;
}

/* above 4.5 kHz the map's 4-bit samples are only noise while the voice is voiced: kept where they are loud next
to it (an /s/) */
static void cryo_line_clean(
	short *line,
	long count)
{
	double voice_ring[CRYO_LINE_HISS_WINDOW] = { 0.0 };
	double hiss_ring[CRYO_LINE_HISS_WINDOW] = { 0.0 };
	double gain_ring[CRYO_LINE_GAIN_WINDOW] = { 0.0 };
	struct cryo_biquad low[4];
	struct cryo_biquad voice[3];
	struct cryo_biquad hiss[2];
	double voice_sum = 0.0;
	double hiss_sum = 0.0;
	double gain_sum = 0.0;
	long frame;

	cryo_line_filter_new(low, 4, FALSE, 4500.0, CRYO_LINE_RATE);
	cryo_line_filter_new(voice, 2, FALSE, 3500.0, CRYO_LINE_RATE);
	cryo_line_filter_new(voice + 2, 1, TRUE, 200.0, CRYO_LINE_RATE);
	cryo_line_filter_new(hiss, 2, TRUE, 5000.0, CRYO_LINE_RATE);
	for (frame = 0; frame < count; frame++)
	{
		double sample = line[frame];
		double low_sample = cryo_line_filter_run(low, 4, sample);
		double voice_sample = cryo_line_filter_run(voice, 3, sample);
		double hiss_sample = cryo_line_filter_run(hiss, 2, sample);
		long at = frame % CRYO_LINE_HISS_WINDOW;
		double gain;

		voice_sum += voice_sample * voice_sample - voice_ring[at];
		voice_ring[at] = voice_sample * voice_sample;
		hiss_sum += hiss_sample * hiss_sample - hiss_ring[at];
		hiss_ring[at] = hiss_sample * hiss_sample;
		/* (0 at 30 dB under the voice, 1 at 20) */
		gain = log10((MAX(hiss_sum, 0.0) / CRYO_LINE_HISS_WINDOW + 1e-3) /
			(MAX(voice_sum, 0.0) / CRYO_LINE_HISS_WINDOW + 1e-3)) + 3.0;
		at = frame % CRYO_LINE_GAIN_WINDOW;
		gain_sum += PIN(gain, 0.0, 1.0) - gain_ring[at];
		gain_ring[at] = PIN(gain, 0.0, 1.0);
		sample = low_sample + gain_sum / CRYO_LINE_GAIN_WINDOW * (sample - low_sample);
		line[frame] = (short)PIN(floor(sample + 0.5), -32768.0, 32767.0);
	}
}

/* the game's uncompressed format, stereo at 44 kHz, at half the level: a stereo channel plays at twice a mono
one's gain */
static void cryo_line_upsample(
	short const *line,
	long count,
	short *stereo)
{
	struct cryo_biquad low[4];
	long frame;

	cryo_line_filter_new(low, 4, FALSE, 10000.0, 2.0 * CRYO_LINE_RATE);
	for (frame = 0; frame < 2 * count; frame++)
	{
		double sample = cryo_line_filter_run(low, 4, frame % 2 ? 0.0 : line[frame / 2]);

		stereo[2 * frame] = stereo[2 * frame + 1] = (short)PIN(floor(sample + 0.5), -32768.0, 32767.0);
	}
}

static long cryo_line_make(
	void)
{
	long tag_index = tag_loaded(SOUND_DEFINITION_TAG, CRYO_LINE_SOUND);
	byte const *words = cryo_line_words();
	short *recording;
	short *line;
	short *stereo = NULL;
	long frame_count = 0;
	short run_index;

	if (tag_index == NONE || !words || !(recording = cryo_line_recording(sound_definition_get(tag_index))))
		return NONE;
	for (run_index = 0; run_index < NUMBEROF(cryo_line_runs); run_index++)
		frame_count += cryo_line_runs[run_index].end - cryo_line_runs[run_index].start;
	if ((line = malloc(frame_count * sizeof(*line))) != NULL)
	{
		frame_count = cryo_line_splice(recording, words, line);
		cryo_line_clean(line, frame_count);
		if ((stereo = malloc(4 * frame_count * sizeof(*stereo))) != NULL)
			cryo_line_upsample(line, frame_count, stereo);
		free(line);
	}
	free(recording);
	if (!stereo)
		return NONE;
	if (!loose_sounds_replace(tag_index, (byte *)stereo, 4 * frame_count * sizeof(*stereo), CRYO_COMPRESSION_NONE,
		CRYO_ENCODING_STEREO, CRYO_SAMPLE_RATE_44K, frame_count * 1000 / CRYO_LINE_RATE))
	{
		free(stereo);
		return NONE;
	}
	error(_error_silent, "co-op: Keyes' line for the cryo bay is %ld ms", frame_count * 1000 / CRYO_LINE_RATE);
	return tag_index;
}

/* ---------- public code */

long coop_cryo_line_definition(
	void)
{
	if (!coop_cryo_line.tried)
	{
		coop_cryo_line.tried = TRUE;
		coop_cryo_line.definition_index = cryo_line_make();
		if (coop_cryo_line.definition_index == NONE)
			error(_error_silent, "co-op: Keyes' line for the cryo bay was not made");
	}
	return coop_cryo_line.definition_index;
}

void coop_cryo_line_release(
	void)
{
	coop_cryo_line.tried = FALSE;
	coop_cryo_line.definition_index = NONE;
}
