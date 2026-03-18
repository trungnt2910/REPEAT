int global_val = 0;
__attribute__((noinline)) void use(int x)
{
    global_val = x;
}

int huge_func(int a)
{
    int local = a + 5;
    use(local);
    __asm__ __volatile__(".rept 70000\n\t"
                         "nop\n\t"
                         ".endr");
    use(local);
    return local;
}
