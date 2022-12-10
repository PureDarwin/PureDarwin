extern void checkInitOrder(int expected);

__attribute__((constructor))
static void myInit()
{
    checkInitOrder(5);
}
