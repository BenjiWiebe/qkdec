#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sndfile.h>
#include <string.h>
#include <stdbool.h>

#define SAMP_RATE	8000.0	//Hz, sampling rate
#define GBLOCK_N	499.0		//SAMP_RATE / N: bin width
#define TARGET1		706
#define TARGET2		433

// Set one of these to 1 if you want qkdec to print the magnitude of that signal at the end of each block, i.e. to plot.
#ifndef DBG_TARGET1
#  define DBG_TARGET1	0
#endif
#ifndef DBG_TARGET2
#  define DBG_TARGET2	0
#endif

const bool silent = false;

struct goertzel_constants {
	double cosine;
	double sine;
	double coeff;
	size_t block_n;
};

struct goertzel_runtime {
	struct goertzel_constants *constants;
	double s1, s2;
	size_t i; // Which sample in the block we are at
};

void calculate_constants(int target_hz, struct goertzel_constants *constants)
{
	int k = (GBLOCK_N * target_hz) / SAMP_RATE + 0.5;
	double w = (2 * M_PI / GBLOCK_N) * k;
	constants->cosine = cos(w);
	constants->sine = sin(w);
	constants->coeff = 2 * constants->cosine;
	constants->block_n = GBLOCK_N;
}

// Returns 1 if block is done, 0 if not yet done. If block is done, *magnitude will be set to the magnitude.
int run_goertzel(double sample, struct goertzel_runtime *rt, double *magnitude)
{
	double s0 = rt->constants->coeff * rt->s1 - rt->s2 + sample;
	rt->s2 = rt->s1;
	rt->s1 = s0;
	rt->i++;

	if(rt->i == rt->constants->block_n)
	{
		*magnitude = sqrt(rt->s1 * rt->s1 + rt->s2 * rt->s2 - rt->s1 * rt->s2 * rt->constants->coeff);
		rt->s1 = 0;
		rt->s2 = 0;
		rt->i = 0;
		return 1;
	}
	return 0;
}

#define TONE_LENGTH_ERROR	0.10
#define DELTA_RING_LEN		5
#define THRESHOLD			50
int check_for_tones(double *mag1, int msec1, double *mag2, int msec2, size_t mag_count)
{
	//size_t blocks_per_sec = SAMP_RATE / GBLOCK_N + 1;
	double delta_ring[DELTA_RING_LEN] = {0};
	int dr_i = 0;
	int needtone1 = 1, needtone2 = 1;
	int msecs_per_block = GBLOCK_N / SAMP_RATE * 1000;
	size_t highn1 = 0, highn2 = 0;
	for(size_t tmp = 0; tmp < mag_count; tmp++)
	{
		double delta = mag1[tmp] - mag2[tmp];
		delta_ring[dr_i++] = delta;
		if(dr_i == DELTA_RING_LEN)
			dr_i = 0;
		double dr_avg = 0;
		for(int i = 0; i < DELTA_RING_LEN; i++)
		{
			dr_avg += delta_ring[i];
		}
		dr_avg = dr_avg / DELTA_RING_LEN;
		if(fabs(dr_avg) < THRESHOLD)
		{
			// a baseline sample causes a reset
			highn1=highn2=0;
			continue;
		}
		if(dr_avg > 0)
		{
			highn1++;
		}
		if(dr_avg < 0)
		{
			highn2++;
		}
		if(highn1 * msecs_per_block > msec1 - msec1 * TONE_LENGTH_ERROR) // If it's no more than TONE_LENGTH_ERROR% shorter, count it as a successful tone detection!
		{
			if(needtone1)
			{
				if(!silent)
					printf("TONE 1 DETECTED\n");
				needtone1 = 0;
			}
		}
		else if(highn2 * msecs_per_block > msec2 - msec2 * TONE_LENGTH_ERROR)
		{
			if(needtone2)
			{
				if(!silent)
					printf("TONE 2 DETECTED\n");
				needtone2 = 0;
			}
		}
	}
	if(!needtone2 && !needtone1)
	{
		if(!silent)
			printf("BOTH TONES DETECTED!\n");
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		printf("Usage: %s <input file>\n", argv[0]);
		return 1;
	}
	char *infile = argv[1];
	struct stat sbuf;
	if(stat(infile, &sbuf) != 0)
	{
		perror("stat");
		return 1;
	}
	if(sbuf.st_size == 0)
	{
		fprintf(stderr, "File %s is empty.\n", infile);
		return 1;
	}

	SF_INFO sndin_info = {0};
	SNDFILE *sndin = sf_open(infile, SFM_READ, &sndin_info);
	if(sndin == NULL)
	{
		fprintf(stderr, "Opening %s failed: %s\n", infile, sf_strerror(NULL));
		return 1;
	}
	if(sndin_info.channels != 1)
	{
		fprintf(stderr, "File %s has %d channels. Only 1 channel supported.\n", infile, sndin_info.channels);
		return 1;
	}
	if(sndin_info.samplerate != 8000)
	{
		fprintf(stderr, "File %s has sampling rate of %d Hz. Only 8000 Hz rate supported.\n", infile, sndin_info.samplerate);
		return 1;
	}

	struct goertzel_constants constants, constants2;
	calculate_constants(TARGET1, &constants);
	calculate_constants(TARGET2, &constants2);

	struct goertzel_runtime runtime = {0}, runtime2 = {0};
	runtime.constants = &constants;
	runtime2.constants = &constants2;

	// FIXME this code assumes both run_goertzel calls use the same GBLOCK_N
	size_t num_blocks = sndin_info.frames / GBLOCK_N + 1;
	double *magnitude1 = malloc(num_blocks * sizeof(double));
	double *magnitude2 = malloc(num_blocks * sizeof(double));
	size_t mag_index = 0;

	if(magnitude1 == NULL || magnitude2 == NULL)
	{
		perror("malloc");
		return 1;
	}

	short sample_;
	while(sf_read_short(sndin, &sample_, 1) == 1)
	{
		int r = 0;
		double sample = ((unsigned short)sample_) / 32768.0;
		// FIXME this code assumes both run_goertzel calls use the same GBLOCK_N within their runtime->constants
		r = run_goertzel(sample, &runtime, &magnitude1[mag_index]);
		r = run_goertzel(sample, &runtime2, &magnitude2[mag_index]);
		if(r) // only increment index, (print magnitude) if a result was actually written!
		{
			if(DBG_TARGET1)
				printf("%f\n", (float)magnitude1[mag_index]);
			if(DBG_TARGET2)
				printf("%f\n", (float)magnitude2[mag_index]);
			mag_index++;
		}
	}

	if(check_for_tones(magnitude1, 1000, magnitude2, 3000, mag_index))
	{
		if(!silent)
			printf("Page found in file %s\n", infile);
		return 0;
	}

	free(magnitude1);
	free(magnitude2);
	sf_close(sndin);

	// 42 = ENOMSG = "No message of desired type"
	// Seems appropriate.
	return 42;
}
