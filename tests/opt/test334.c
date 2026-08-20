struct Bits {
    unsigned char *bytes;
};

int main(void) {
    unsigned char buf[8];
    struct Bits bits;
    int pg = 21;

    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    buf[4] = 0;
    buf[5] = 0;
    buf[6] = 0;
    buf[7] = 0;

    bits.bytes = buf;
    bits.bytes[pg / 8] = bits.bytes[pg / 8] | (1 << (pg & 7));

    return bits.bytes[2] + 10;
}
