int target_func(int a)
{
    return a;
}

void test_func()
{
    int (*func_ptr)(int) = target_func;
}
