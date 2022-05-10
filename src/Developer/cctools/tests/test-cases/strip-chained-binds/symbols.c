#ifndef DYLIB
int global_sym;
#endif /* DYLIB */

extern int entry(void);

static int private_sym(void)
{
  return 1;
}

#ifdef DYLIB
int entry(void)
{
#else
int main(void)
{
  global_sym = 
#endif /* DYLIB */
#ifdef DYLIB_CLIENT
    entry();
#else
    private_sym();
#endif /* DYLIB_CLIENT */
  return 0;
}
