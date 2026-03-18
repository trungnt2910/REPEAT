int callee(int a);

int normal_func(int x)
{
    return callee(x) + 5;
}
