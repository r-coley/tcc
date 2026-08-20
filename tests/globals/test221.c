/* Regression: brace-initialized global char arrays must preserve embedded zeroes. */
char bytes[3] = { 1, 0, 2 };

int main(void) {
    return bytes[0] + bytes[1] + bytes[2] + 39;
}
