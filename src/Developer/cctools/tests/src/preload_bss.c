#include <stdint.h>

const uint32_t data_addr = 0;
const uint32_t data_size = 0;

int main(void)
{
  static char zero[32];
  int i;
  for (i = 0; zero[i]; ++i);
  return i;
}
