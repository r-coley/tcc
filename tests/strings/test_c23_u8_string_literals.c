#include <stddef.h>

int
main(void)
{
    char *s = u8"ok";
    char *adj = u8"he" u8"llo";
    char *nul = u8"a\0b";

    if (sizeof(u8"ok") != 3)
        return 1;
    if (s[0] != 'o' || s[1] != 'k' || s[2] != 0)
        return 2;

    if (sizeof(u8"he" u8"llo") != 6)
        return 3;
    if (adj[0] != 'h' || adj[1] != 'e' || adj[2] != 'l' ||
        adj[3] != 'l' || adj[4] != 'o' || adj[5] != 0)
        return 4;

    if (sizeof(u8"a\0b") != 4)
        return 5;
    if (nul[0] != 'a' || nul[1] != 0 || nul[2] != 'b' || nul[3] != 0)
        return 6;

    return 42;
}
