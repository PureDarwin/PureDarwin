#include <corecrypto/ccrng.h>
#include <stdio.h>
#include <corecrypto/ccrng_system.h>

struct ccrng_system_state ccrng_global_system_rng_instance = {0};

struct ccrng_state* ccrng(int* error) {
	// we're basically using `fd` as a boolean for whether it's initialized or not
	// i mean, it's not like our implementation of the system RNG is using it ¯\_(ツ)_/¯
	if (ccrng_global_system_rng_instance.fd == 0) {
		if (ccrng_system_init(&ccrng_global_system_rng_instance)) {
			if (error)
				*error = 1;
			return NULL;
		}
		ccrng_global_system_rng_instance.fd = 1;
	}

	if (error)
		*error = 0;

	return (struct ccrng_state*)&ccrng_global_system_rng_instance;
};

int ccrng_uniform(struct ccrng_state *rng, uint64_t bound, uint64_t *rand) {
	// TODO(@facekapow): make this a proper uniform RNG
	// (actually, the whole RNG system needs to be fixed)
	//
	// the current implementation for this function does at least satisfy the requirement that the value be
	// between 0 and the upper bound, but i wouldn't say the number it generates has been "uniformly generated"

	uint64_t tmp = 0;
	ccrng_generate(rng, sizeof(tmp), &tmp);
	*rand = tmp % bound;
	return 0;
};
