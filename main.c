#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>

#define SAMP_RATE	8000	//Hz, sampling rate
#define GBLOCK_N	499		//SAMP_RATE / N: bin width
#define TARGET		706

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

	struct goertzel_constants constants;
	calculate_constants(706, &constants);

	struct goertzel_runtime runtime = {0};
	runtime.constants = &constants;

	// Little-endian 16 bits signed

	int16_t *samp_data = (int16_t*) mfile;
	size_t num_samples = fsize / sizeof(int16_t);
	double magnitude = 0;

	for(size_t i = 0; i < num_samples; i++)
	{
		double sample = le16toh(samp_data[i]) / 32768.0;
		if(run_goertzel(sample, &runtime, &magnitude))
		{
			printf("%f\n", (float)magnitude);
		}
	}


	return 0;
}



