#define MS_ABI __attribute__((ms_abi))

static int elf_static_func(int a)
{
    return a + 10;
}

MS_ABI int get_val(int a)
{
    return elf_static_func(a) + 32;
}

int my_global = 100;

MS_ABI int* get_global_ptr()
{
    return &my_global;
}

// Test entry points for verifying call flow from PE-Native to Translated ELF.
MS_ABI int elf_add(int a, int b)
{
    return a + b;
}

// Verify dynamic unresolved import resolution when Translated ELF calls PE-Native.
extern MS_ABI int pe_native_multiply(int a, int b);

MS_ABI int elf_call_pe(int a)
{
    return pe_native_multiply(a, 3);
}
