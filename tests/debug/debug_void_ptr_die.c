/* Regression test for DWARF void pointer type DIE emission.
   A void * pointer type must not emit DW_AT_type as CU offset 0. */

void *debug_void_ptr_identity(void *p)
{
    return p;
}

int main(void)
{
    int value = 42;
    void *p = &value;
    return debug_void_ptr_identity(p) == p ? 0 : 1;
}
