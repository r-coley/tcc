#include <stddef.h>

#pragma pack(1)
struct Packed1 {
    char c;
    int i;
};

#pragma pack(push, 2)
struct Packed2 {
    char c;
    int i;
};
#pragma pack(pop)

struct PackedRestored {
    char c;
    int i;
};
#pragma pack()

int
main(void)
{
    int fail = 0;

    if (offsetof(struct Packed1, c) != 0) fail++;
    if (offsetof(struct Packed1, i) != 1) fail++;

    if (offsetof(struct Packed2, c) != 0) fail++;
    if (offsetof(struct Packed2, i) != 2) fail++;

    if (offsetof(struct PackedRestored, c) != 0) fail++;
    if (offsetof(struct PackedRestored, i) != 1) fail++;

    return fail;
}
