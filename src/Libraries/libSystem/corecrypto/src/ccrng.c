#include <corecrypto/ccrng_system.h>
#include <stdint.h>

struct ccrng_system_state ccrng_global_system_rng_instance = {0};

struct ccrng_state* ccrng(int* error) {
	// we're basically using `fd` as a boolean for whether it's initialized or not
	// i mean, it's not like our implementation of the system RNG is using it.
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
	uint64_t min;
	uint64_t tmp;
	int rc;

	if (bound < 2) {
		*rand = 0;
		return 0;
	}

	min = (uint64_t)(-bound) % bound;
	do {
		tmp = 0;
		rc = ccrng_generate(rng, sizeof(tmp), &tmp);
		if (rc != 0) {
			return rc;
		}
	} while (tmp < min);

	*rand = tmp % bound;
	return 0;
};
