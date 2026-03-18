static int local_var = 10;
int global_var = 20;
static int local_func()
{
    return 1;
}
int global_func()
{
    return local_func();
}
