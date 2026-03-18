static inline __attribute__((always_inline)) int add_inline(int x, int y)
{
    int sum = x + y;
    return sum;
}

int call_add(int a)
{
    return add_inline(a, 10);
}
