/* Regression: arithmetic compound assignment operators inside macro bodies
 * must be tokenized as single tokens (not split) by the preprocessor.
 * Bug: *=, +=, -=, /=, %= were each split into two tokens (* =, + =, etc.)
 * causing "Expected expression factor near TOK_ASSIGN" in stage1.
 */
#define DOUBLE(x)    do { (x) *= 2; } while (0)
#define HALVE(x)     do { (x) /= 2; } while (0)
#define ADD5(x)      do { (x) += 5; } while (0)
#define SUB3(x)      do { (x) -= 3; } while (0)
#define MOD7(x)      do { (x) %= 7; } while (0)

int main(void)
{
    int v = 3;
    DOUBLE(v);   /* 6  */
    DOUBLE(v);   /* 12 */
    HALVE(v);    /* 6  */
    ADD5(v);     /* 11 */
    SUB3(v);     /* 8  */
    MOD7(v);     /* 1  */
    DOUBLE(v);   /* 2  */
    ADD5(v);     /* 7  */
    HALVE(v);    /* 3 — but integer divide */
    /* 3 != 42, chain further */
    v = 1;
    DOUBLE(v);   /* 2  */
    DOUBLE(v);   /* 4  */
    DOUBLE(v);   /* 8  */
    ADD5(v);     /* 13 */
    DOUBLE(v);   /* 26 */
    ADD5(v);     /* 31 */
    DOUBLE(v);   /* 62 */
    SUB3(v);     /* 59 */
    MOD7(v);     /* 3  -- 59 % 7 = 3 */
    /* restart for clean result */
    v = 20;
    HALVE(v);    /* 10 */
    DOUBLE(v);   /* 20 */
    ADD5(v);     /* 25 */
    MOD7(v);     /* 4  */
    v = 6;
    DOUBLE(v);   /* 12 */
    DOUBLE(v);   /* 24 */
    ADD5(v);     /* 29 */
    SUB3(v);     /* 26 */
    MOD7(v);     /* 5  -- 26 % 7 = 5 */
    v = 20;
    ADD5(v);     /* 25 */
    MOD7(v);     /* 4  */
    v = 40;
    ADD5(v);     /* 45 */
    SUB3(v);     /* 42 */
    return (v == 42) ? 42 : 1;
}
