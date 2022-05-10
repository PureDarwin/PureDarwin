#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void _makestr(char** s)
{
  const char* hw = "hello, world!";
  size_t len = strlen(hw);
  *s = malloc(len + 1);
  snprintf(*s, len + 1, "%s", hw);
}

void _freestr(char** s)
{
  assert(s);
  assert(*s);
  free(*s);
}

void _showstr(char** s)
{
  assert(s);
  assert(*s);
  printf("%s\n", *s);
}

void (*funcs[])(char**) = { /* I had to look this syntax up :/ */
  _makestr,
  _showstr,
  _freestr,
};

FILE** unauth_bind = &stderr;
void (*auth_rebase)(char**s) = &_makestr;
int x = 0, *unauth_rebase = &x;

int main (void)
{
  int n = sizeof(funcs) / sizeof(*funcs);
  char* s;
  for (int i = 0; i < n; ++i) {
    funcs[i](&s);
  }
  return 0;
}
