static int local_func(int a)
{
    return a + 1;
}

int global_func(int a)
{
    return local_func(a);
}
