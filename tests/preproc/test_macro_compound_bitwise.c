/* Regression: bitwise compound assignment and shift operators in macro bodies.
 * &=, |=, ^=, <<=, >>= were all split by the preprocessor.
 */
#define SET_BITS(x, mask)   do { (x) |= (mask); } while (0)
#define CLR_BITS(x, mask)   do { (x) &= ~(mask); } while (0)
#define XOR_BITS(x, mask)   do { (x) ^= (mask); } while (0)
#define SHL(x, n)           do { (x) <<= (n); } while (0)
#define SHR(x, n)           do { (x) >>= (n); } while (0)

int main(void)
{
    int v = 0;
    SET_BITS(v, 0xFF);   /* 255 */
    CLR_BITS(v, 0x0F);   /* 240 = 0xF0 */
    XOR_BITS(v, 0xAA);   /* 0xF0 ^ 0xAA = 0x5A = 90 */
    SHL(v, 2);           /* 360 */
    SHR(v, 1);           /* 180 */
    CLR_BITS(v, 0x80);   /* 180 & ~0x80 = 52 */
    SET_BITS(v, 0x0A);   /* 52 | 10 = 62 */
    XOR_BITS(v, 0x14);   /* 62 ^ 20 = 42 */
    return (v == 42) ? 42 : 1;
}
