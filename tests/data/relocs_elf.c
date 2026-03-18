static int local_var = 100;
int* local_ptr = &local_var; // Triggers R_*_RELATIVE (relative dynamic reloc)

int global_var = 200;
int* global_ptr = &global_var; // Triggers symbolic dynamic reloc pointing to defined symbol

int* get_global_ptr()
{
    return &global_var;
}

extern int external_func(void);

// Triggers R_*_JUMP_SLOT / R_*_GLOB_DAT undefined symbol
int call_ext()
{
    return external_func() + 5;
}
