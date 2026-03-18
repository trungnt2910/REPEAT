int test_func(int param_a)
{
    int local_stack = param_a + 5;
    register int local_reg = param_a - 5;
    return local_stack + local_reg;
}
