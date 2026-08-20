_Alignas(16) int global_aligned;

struct AlignedStruct {
    char c;
    _Alignas(16) int x;
    char d;
};

int main(void)
{
    _Alignas(16) int local_aligned = 7;
    struct AlignedStruct value;
    unsigned long global_addr = (unsigned long)&global_aligned;
    unsigned long local_addr = (unsigned long)&local_aligned;
    unsigned long field_offset = (unsigned long)((char *)&value.x - (char *)&value);

    if ((global_addr & 15UL) != 0)
        return 1;
    if ((local_addr & 15UL) != 0)
        return 2;
    if (_Alignof(struct AlignedStruct) != 16)
        return 3;
    if (field_offset != 16UL)
        return 4;
    if (sizeof(struct AlignedStruct) != 32)
        return 5;

    return 42;
}
