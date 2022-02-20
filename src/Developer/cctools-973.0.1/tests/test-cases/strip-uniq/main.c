extern int foo(void);
extern int bar(void);

int main(void)
{
  if (0 == foo() && 0 == bar())
    return 0;
    
  return 1;
}