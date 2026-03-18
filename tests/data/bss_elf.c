int zero_global = 0;  // Placed in BSS section to test BSS global variables.
int init_global = 10; // Placed in Data section to test initialized global variables.

int get_val()
{
    return zero_global + init_global;
}
