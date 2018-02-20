#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#define SAMP_RATE	8000	//Hz, sampling rate
#define GBLOCK_N	499		//SAMP_RATE / N: bin width
#define TARGET1		706
#define TARGET2		433

// Set one of these to 1 if you want qkdec to print the magnitude of that signal at the end of each block, i.e. to plot.
#ifndef DBG_TARGET1
#  define DBG_TARGET1	0
#endif
#ifndef DBG_TARGET2
#  define DBG_TARGET2	0
#endif

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

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		printf("Usage: %s <input file>\n", argv[0]);
		return 1;
	}
	char *infile = argv[1];
	struct stat sbuf;
	off_t fsize;
	void *mfile;
	int fd = open(infile, O_RDONLY);
	if(fd == -1)
	{
		perror("open");
		return 1;
	}
	if(fstat(fd, &sbuf) != 0)
	{
		perror("stat");
		return 1;
	}
	fsize = sbuf.st_size;
	mfile = mmap(NULL, fsize, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
	if(mfile == MAP_FAILED)
	{
		perror("mmap");
		return 1;
	}

	struct goertzel_constants constants, constants2;
	calculate_constants(TARGET1, &constants);
	calculate_constants(TARGET2, &constants2);

	struct goertzel_runtime runtime = {0}, runtime2 = {0};
	runtime.constants = &constants;
	runtime2.constants = &constants2;

	// Little-endian 16 bits signed

	int16_t *samp_data = (int16_t*) mfile;
	size_t num_samples = fsize / sizeof(int16_t);

	// FIXME this code assumes both run_goertzel calls use the same GBLOCK_N
	size_t num_blocks = num_samples / GBLOCK_N + 1;
	double *magnitude1 = malloc(num_blocks * sizeof(double));
	double *magnitude2 = malloc(num_blocks * sizeof(double));
	size_t mag_index = 0;

	if(magnitude1 == NULL || magnitude2 == NULL)
	{
		perror("malloc");
		return 1;
	}

	for(size_t i = 0; i < num_samples; i++)
	{
		int r = 0;
		double sample = le16toh(samp_data[i]) / 32768.0;
		// FIXME this code assumes both run_goertzel calls use the same GBLOCK_N within their runtime->constants
		r = run_goertzel(sample, &runtime, &magnitude1[mag_index]);
		if(DBG_TARGET1)
			printf("%f\n", (float)magnitude1[mag_index]);
		r = run_goertzel(sample, &runtime2, &magnitude2[mag_index]);
		if(DBG_TARGET2)
			printf("%f\n", (float)magnitude2[mag_index]);
		if(r) // only increment index if a result was actually written!
			mag_index++;
	}

	free(magnitude1);
	free(magnitude2);
	close(fd);

	return 0;
}



