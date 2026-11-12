# PDDRMmsm host tests: the driver's a6xx, gmu and smmu-500 code against a fake soc, on linux
# make check builds and runs every suite, PD_MSM_TEST_VERBOSE=1 shows the driver's own log
# leaks are counted by the fakes (the driver has no teardown yet), so leak sanitizer stays off
CC ?= cc
CFLAGS ?= -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
	-Wno-missing-field-initializers -Werror=implicit-function-declaration \
	-DPD_MSM_HOST_TEST -I. -Ishim -Ishim/include -I.. -include shim/pd_msm_host.h

SUITES = test_smmu500 test_a6xx
COMMON = fake.c fake_msm.c

all: $(SUITES)

test_%: test_%.c $(COMMON) fake.h test.h shim/pd_msm_host.h $(wildcard ../*.c ../*.h)
	$(CC) $(CFLAGS) -o $@ $< $(COMMON)

check: $(SUITES)
	@fail=0; for t in $(SUITES); do ASAN_OPTIONS=detect_leaks=0 ./$$t || fail=1; done; exit $$fail

clean:
	rm -f $(SUITES)

.PHONY: all check clean
