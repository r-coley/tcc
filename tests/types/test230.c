/* long via typedef: time_t-style usage. Must be 64-bit on arm64.
   Regression: long was treated as 4-byte int, corrupting 64-bit values. */
typedef long mylong;

mylong add_longs(mylong a, mylong b) {
    return a + b;
}

int main() {
    mylong x = 20;
    mylong y = 22;
    return (int)add_longs(x, y);
}
