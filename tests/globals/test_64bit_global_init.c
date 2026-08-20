long long sg = 0x1122334455667788LL;
unsigned long long ug = 0xFEDCBA9876543210ULL;
unsigned long long arr[3] = { 1ULL, 0x100000000ULL, 0x8000000000000000ULL };

struct G64 {
    int tag;
    unsigned long long value;
};

struct G64 g = { 7, 0x123456789ABCDEF0ULL };

int main(void)
{
    if (sg != 0x1122334455667788LL) return 1;
    if (ug != 0xFEDCBA9876543210ULL) return 2;
    if (arr[0] != 1ULL) return 3;
    if (arr[1] != 0x100000000ULL) return 4;
    if (arr[2] != 0x8000000000000000ULL) return 5;
    if (g.tag != 7) return 6;
    if (g.value != 0x123456789ABCDEF0ULL) return 7;
    return 42;
}
