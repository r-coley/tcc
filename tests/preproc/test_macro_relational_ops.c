/* Regression: relational and logical operators inside macro bodies.
 * &&, ||, ==, !=, <=, >=, ++, --, -> must survive macro expansion as single tokens.
 */
#define AND(a, b)       ((a) && (b))
#define OR(a, b)        ((a) || (b))
#define EQ(a, b)        ((a) == (b))
#define NEQ(a, b)       ((a) != (b))
#define GE(a, b)        ((a) >= (b))
#define LE(a, b)        ((a) <= (b))
#define INC(x)          (++(x))
#define DEC(x)          (--(x))
#define SHL2(x)         ((x) << 2)
#define SHR1(x)         ((x) >> 1)

typedef struct { int val; } Box;
#define BOX_VAL(p)      ((p)->val)

int main(void)
{
    int a = 5, b = 5;

    if (!EQ(a, b))     return 1;
    if (NEQ(a, b))     return 1;
    if (!GE(a, b))     return 1;
    if (!LE(a, b))     return 1;
    if (!AND(1, 1))    return 1;
    if (AND(1, 0))     return 1;
    if (!OR(0, 1))     return 1;
    if (OR(0, 0))      return 1;

    INC(a);            /* a = 6 */
    DEC(b);            /* b = 4 */
    if (!GE(a, b))     return 1;
    if (LE(a, b))      return 1;
    if (EQ(a, b))      return 1;
    if (!NEQ(a, b))    return 1;

    int v = 1;
    v = SHL2(v);       /* 4 */
    v = SHL2(v);       /* 16 */
    v = SHR1(v);       /* 8 */
    v = SHL2(v);       /* 32 */
    INC(v);            /* 33 */
    INC(v);            /* 34 */
    v = SHR1(v);       /* 17 */
    v = SHL2(v);       /* 68 */
    DEC(v);            /* 67 */
    DEC(v);            /* 66 */
    DEC(v);            /* 65 */
    DEC(v);            /* 64 */
    v = SHR1(v);       /* 32 */
    INC(v);            /* 33 */
    INC(v);            /* 34 */
    INC(v);            /* 35 */
    INC(v);            /* 36 */
    INC(v);            /* 37 */
    INC(v);            /* 38 */
    INC(v);            /* 39 */
    INC(v);            /* 40 */
    INC(v);            /* 41 */
    INC(v);            /* 42 */

    Box box;
    box.val = v;
    Box *p = &box;
    return (BOX_VAL(p) == 42) ? 42 : 1;
}
