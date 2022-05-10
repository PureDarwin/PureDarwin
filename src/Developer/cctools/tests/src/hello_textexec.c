#include <stdio.h>

__attribute__ ((section ("__TEXT_EXEC,__text,regular,pure_instructions")))
static void hello(void)
{
  printf("hello, world!\n");
}

int main(void)
{
  hello();
  return 0;
}
