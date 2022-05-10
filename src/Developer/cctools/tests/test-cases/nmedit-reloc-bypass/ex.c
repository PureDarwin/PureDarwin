int my_extern(void)
{
  return 1;
}

int my_local(void)
{
  return -1;
}

int main(void)
{
  return my_extern() + my_local();
}
