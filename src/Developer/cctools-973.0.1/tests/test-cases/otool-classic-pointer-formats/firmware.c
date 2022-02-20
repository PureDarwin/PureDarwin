int x = 0;
int* g = &x;

int y[1000] = {1};
int* h = &y[1];
int* i = &y[999];

int start(void)
{
  return *g;
}
