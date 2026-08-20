alignas(16) int global_aligned;

int main(void)
{
    alignas(16) int local_aligned = 11;

    if (((unsigned long)&global_aligned & 15UL) != 0)
        return 1;
    if (((unsigned long)&local_aligned & 15UL) != 0)
        return 2;
    if (alignof(global_aligned) != 16)
        return 3;
    if (alignof(local_aligned) != 16)
        return 4;

    return 42;
}
