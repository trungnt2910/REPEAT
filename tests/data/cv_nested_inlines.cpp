static inline __attribute__((always_inline)) int inner_inline(int z)
{
    int inner_val = z + 5;
    if (inner_val > 10)
    {
        int lexical_val = inner_val * 2;
        return lexical_val;
    }
    return inner_val;
}

static inline __attribute__((always_inline)) int outer_inline(int x, int y)
{
    int sum = x + y;
    int res = inner_inline(sum);
    return res;
}

int call_nested(int a)
{
    return outer_inline(a, 10);
}
