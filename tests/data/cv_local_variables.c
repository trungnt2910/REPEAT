static int file_static_var = 42;

int get_static_var(void)
{
    static int func_static_var = 84;
    return file_static_var + func_static_var;
}
