#include <stdint.h>

//#if defined(__LP64__)
//const uint64_t data_addr = 0;
//const uint32_t data_size = 0;
//#else
const uint32_t data_addr = 0;
const uint32_t data_size = 0;
//#endif

char hello[32] = "hello\n";

int main(void)
{
  static char zero[32];
  int i;
  for (i = 0; hello[i]; ++i);
  return i;
}
