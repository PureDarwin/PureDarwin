// _strchrnul: declared in <string.h> but never implemented here,
// so configure scripts detect it and the link then fails (gnum4, nano)
#include <stddef.h>

char *
strchrnul(const char *s, int c)
{
	char ch = (char)c;

	while (*s != '\0' && *s != ch) {
		s++;
	}
	return (char *)s;
}
