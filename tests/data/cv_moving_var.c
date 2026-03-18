int global_val = 0;

__attribute__((noinline)) void use(int x)
{
    global_val = x;
}

int test_func(int a, int b)
{
    int local = a + 5;
    use(local);
    local = local * b;
    use(local);
    return local;
}
