/*
 * temp: print the SoC temperature from machdep.temperature (millidegrees Celsius).
 *   temp        -> 42.3 °C
 *   temp -m     -> 42300
 */
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>

int
main(int argc, char **argv)
{
	int mc = 0;
	size_t len = sizeof(mc);

	if (sysctlbyname("machdep.temperature", &mc, &len, NULL, 0) != 0) {
		perror("machdep.temperature");
		return 1;
	}
	if (argc > 1 && strcmp(argv[1], "-m") == 0) {
		printf("%d\n", mc);
	} else {
		printf("%d.%d \xc2\xb0""C\n", mc / 1000, (mc < 0 ? -mc : mc) % 1000 / 100);
	}
	return 0;
}
