volatile int g1 = 1;
volatile int g2 = 2;
volatile int g3 = 3;
volatile int g4 = 4;
volatile int g5 = 5;
volatile int g6 = 6;
volatile int g7 = 7;
volatile int g8 = 8;

int global_val = 0;
__attribute__((noinline)) void use(int x)
{
    global_val = x;
}
__attribute__((noinline)) void use_many(int a, int b, int c, int d, int e, int f, int g, int h)
{
    global_val = a + b + c + d + e + f + g + h;
}

int test_spill(int a, int b)
{
    int local = a + 5;
    use(local);

    int v1 = g1, v2 = g2, v3 = g3, v4 = g4, v5 = g5, v6 = g6, v7 = g7, v8 = g8;
    use_many(v1, v2, v3, v4, v5, v6, v7, v8);

    local = local * b;
    use(local);

    return local + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8;
}
