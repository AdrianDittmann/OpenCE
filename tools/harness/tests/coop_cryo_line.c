/*
COOP_CRYO_LINE.C (test)

The words, the splice and the clean-up of Keyes' line for network co-op's
cryo bay (port/linux/game/coop_cryo_line.c), from the embedded words and
made-up recordings (test_coop_cryo_line.py).
*/

#include "harness.h"

#include <math.h>

typedef unsigned short word;

#define NUMBEROF(array) ((long)(sizeof(array) / sizeof((array)[0])))
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define PIN(n, floor, ceiling) ((n) < (floor) ? (floor) : MIN((n), (ceiling)))

#include "words.inc"
#include "under_test.inc"

static short recording[CRYO_LINE_RECORDING_FRAMES];
static short line[CRYO_LINE_RECORDING_FRAMES + CRYO_LINE_WORDS_FRAMES];
static short stereo[4 * CRYO_LINE_RATE];
static double measured[2 * CRYO_LINE_RATE];

static short words_sample(byte const *words, long frame)
{
	return (short)(words[2 * frame] | (words[2 * frame + 1] << 8));
}

/* of `frequency` in measured[0..count), a whole number of its cycles at `rate` */
static double amplitude(long count, double frequency, double rate)
{
	double in_phase = 0.0;
	double quadrature = 0.0;
	long index;

	for (index = 0; index < count; index++)
	{
		in_phase += measured[index] * cos(2.0 * M_PI * frequency * index / rate);
		quadrature += measured[index] * sin(2.0 * M_PI * frequency * index / rate);
	}
	return 2.0 * sqrt(in_phase * in_phase + quadrature * quadrature) / count;
}

static double measure(short const *samples, long stride, long first, long count, double frequency, double rate)
{
	long index;

	for (index = 0; index < count; index++)
		measured[index] = samples[(first + index) * stride];
	return amplitude(count, frequency, rate);
}

/* a filter's gain at `frequency` once settled, in dB */
static double response(boolean high, short count, double cutoff, double rate, double frequency)
{
	struct cryo_biquad sections[4];
	long length = (long)rate;
	long frame;

	cryo_line_filter_new(sections, count, high, cutoff, rate);
	for (frame = 0; frame < 2 * length; frame++)
	{
		double out = cryo_line_filter_run(sections, count, sin(2.0 * M_PI * frequency * frame / rate));

		if (frame >= length)
			measured[frame - length] = out;
	}
	return 20.0 * log10(amplitude(length, frequency, rate));
}

/* a second of 150 Hz harmonics to 1950 Hz, each of `voice`, and a 7 kHz tone of `hiss` */
static void voice_and_hiss(double voice, double hiss)
{
	long frame;

	for (frame = 0; frame < CRYO_LINE_RATE; frame++)
	{
		double sample = hiss * sin(2.0 * M_PI * 7000.0 * frame / CRYO_LINE_RATE);
		short harmonic;

		for (harmonic = 1; harmonic <= 13; harmonic++)
			sample += voice * sin(2.0 * M_PI * 150.0 * harmonic * frame / CRYO_LINE_RATE);
		line[frame] = (short)floor(sample + 0.5);
	}
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	byte const *words = cryo_line_words();

	/* the embedded file's samples, and none from a file that is not one */
	CASE("words")
	{
		byte *file = (byte *)coop_cryo_words;
		unsigned long size = coop_cryo_words_size;

		CHECK(words == file + 44, "the words are at %ld in their file, not 44", words ? (long)(words - file) : -1L);
		file[22] = 2;
		CHECK(!cryo_line_words(), "stereo words are taken");
		file[22] = 1;
		coop_cryo_words_size = size - 2;
		CHECK(!cryo_line_words(), "words past the file's end are taken");
		coop_cryo_words_size = size;
		file[40]++;
		CHECK(!cryo_line_words(), "words of another length are taken");
		return 0;
	}

	/* each run whole, each seam blended from the run before into the next */
	CASE("splice")
	{
		long expected = 0;
		long at = 0;
		long frame;
		short run_index;
		long count;

		CHECK(words, "no words");
		for (frame = 0; frame < CRYO_LINE_RECORDING_FRAMES; frame++)
			recording[frame] = (short)((frame * 7919) % 30001 - 15000);
		count = cryo_line_splice(recording, words, line);
		for (run_index = 0; run_index < NUMBEROF(cryo_line_runs); run_index++)
			expected += cryo_line_runs[run_index].end - cryo_line_runs[run_index].start;
		expected -= CRYO_LINE_CROSSFADE * (NUMBEROF(cryo_line_runs) - 1);
		CHECK(count == expected, "the line is %ld frames, not %ld", count, expected);
		for (run_index = 0; run_index < NUMBEROF(cryo_line_runs); run_index++)
		{
			struct cryo_line_run const *run = &cryo_line_runs[run_index];
			struct cryo_line_run const *before = run_index > 0 ? &cryo_line_runs[run_index - 1] : NULL;
			long length = run->end - run->start;

			for (frame = 0; frame < length; frame++)
			{
				long sample = run->words ? words_sample(words, run->start + frame) : recording[run->start + frame];

				if (before && frame < CRYO_LINE_CROSSFADE)
				{
					long tail = before->end - CRYO_LINE_CROSSFADE + frame;
					long from = before->words ? words_sample(words, tail) : recording[tail];

					sample = from + (sample - from) * frame / (CRYO_LINE_CROSSFADE - 1);
				}
				else if (run_index + 1 < NUMBEROF(cryo_line_runs) && frame >= length - CRYO_LINE_CROSSFADE)
				{
					continue;
				}
				CHECK(line[at + frame] - sample <= 1 && sample - line[at + frame] <= 1,
					"frame %ld of run %d is %d in the line at %ld, not %ld", frame, run_index, line[at + frame], at + frame,
					sample);
			}
			at += length - CRYO_LINE_CROSSFADE;
		}
		return 0;
	}

	/* -3 dB at the cutoff, none in the band passed */
	CASE("filters")
	{
		static struct
		{
			boolean high;
			short count;
			double cutoff;
			double rate;
			double passed;
		} const filters[] =
		{
			{ FALSE, 4, 4500.0, CRYO_LINE_RATE, 2250.0 }, { FALSE, 2, 3500.0, CRYO_LINE_RATE, 1750.0 },
			{ TRUE, 1, 200.0, CRYO_LINE_RATE, 800.0 }, { TRUE, 2, 5000.0, CRYO_LINE_RATE, 10000.0 },
			{ FALSE, 4, 10000.0, 2 * CRYO_LINE_RATE, 5000.0 },
		};
		short index;

		for (index = 0; index < NUMBEROF(filters); index++)
		{
			double cutoff = filters[index].cutoff;
			double at_cutoff = response(filters[index].high, filters[index].count, cutoff, filters[index].rate, cutoff);
			double passed = response(filters[index].high, filters[index].count, cutoff, filters[index].rate,
				filters[index].passed);

			CHECK(fabs(at_cutoff + 3.0103) < 0.01, "filter %d is %.3f dB at its cutoff, not -3.01", index, at_cutoff);
			CHECK(fabs(passed) < 0.1, "filter %d is %.3f dB in its band", index, passed);
		}
		return 0;
	}

	/* above 4.5 kHz: a tenth or less where it is far under the voice, all of it where it is over; the voice kept */
	CASE("clean")
	{
		static struct
		{
			double voice;
			double hiss;
			double kept;
		} const signals[] = { { 1000.0, 40.0, 0.1 }, { 300.0, 4000.0, 1.0 } };
		short index;

		for (index = 0; index < NUMBEROF(signals); index++)
		{
			double voice_in;
			double hiss_in;
			double voice_out;
			double hiss_out;

			voice_and_hiss(signals[index].voice, signals[index].hiss);
			voice_in = measure(line, 1, CRYO_LINE_RATE / 2, CRYO_LINE_RATE / 2, 600.0, CRYO_LINE_RATE);
			hiss_in = measure(line, 1, CRYO_LINE_RATE / 2, CRYO_LINE_RATE / 2, 7000.0, CRYO_LINE_RATE);
			cryo_line_clean(line, CRYO_LINE_RATE);
			voice_out = measure(line, 1, CRYO_LINE_RATE / 2, CRYO_LINE_RATE / 2, 600.0, CRYO_LINE_RATE);
			hiss_out = measure(line, 1, CRYO_LINE_RATE / 2, CRYO_LINE_RATE / 2, 7000.0, CRYO_LINE_RATE);
			CHECK(fabs(20.0 * log10(voice_out / voice_in)) < 0.2, "signal %d's voice is %.2f dB", index,
				20.0 * log10(voice_out / voice_in));
			if (signals[index].kept < 1.0)
			{
				CHECK(hiss_out <= signals[index].kept * hiss_in, "signal %d keeps %.1f of %.1f above 4.5 kHz", index,
					hiss_out, hiss_in);
			}
			else
			{
				CHECK(fabs(20.0 * log10(hiss_out / hiss_in)) < 0.1, "signal %d's /s/ is %.2f dB", index,
					20.0 * log10(hiss_out / hiss_in));
			}
		}
		return 0;
	}

	/* twice the rate, the same on both channels, half the level, and no image above 11 kHz */
	CASE("upsample")
	{
		long count = 2 * CRYO_LINE_RATE / 5;
		long frame;
		double level;
		double image;

		for (frame = 0; frame < count; frame++)
			line[frame] = (short)floor(10000.0 * sin(2.0 * M_PI * 1000.0 * frame / CRYO_LINE_RATE) + 0.5);
		cryo_line_upsample(line, count, stereo);
		for (frame = 0; frame < 2 * count; frame++)
		{
			CHECK(stereo[2 * frame] == stereo[2 * frame + 1], "frame %ld is %d and %d", frame, stereo[2 * frame],
				stereo[2 * frame + 1]);
		}
		level = measure(stereo, 2, count, count, 1000.0, 2 * CRYO_LINE_RATE);
		image = measure(stereo, 2, count, count, CRYO_LINE_RATE - 1000.0, 2 * CRYO_LINE_RATE);
		CHECK(fabs(20.0 * log10(level / 10000.0) + 6.0206) < 0.1, "1 kHz is %.2f dB, not -6.02",
			20.0 * log10(level / 10000.0));
		CHECK(image < level / 100.0, "its image at 21.05 kHz is %.1f, 1 kHz %.1f", image, level);
		return 0;
	}

	fprintf(stderr, "no case %s\n", case_name);
	return 2;
}
