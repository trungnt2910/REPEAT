#define MS_ABI __attribute__((ms_abi))

// Declare ELF function to allow PE-native test code to perform cross-calls.
extern MS_ABI int elf_add(int a, int b);

MS_ABI int pe_native_multiply(int a, int b)
{
    return a * b;
}

MS_ABI int pe_call_elf(int a)
{
    return elf_add(a, 10);
}
