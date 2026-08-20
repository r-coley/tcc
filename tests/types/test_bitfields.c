int main(void)
{
    struct Bits {
        unsigned int a : 3;
        unsigned int b : 5;
        unsigned int c : 6;
    };
    struct Bits bits;
    int score = 0;

    bits.a = 5;
    bits.b = 17;
    bits.c = 42;

    score += bits.a == 5;
    score += bits.b == 17;
    score += bits.c == 42;

    bits.b += 7;
    score += bits.b == 24;
    score += bits.a == 5;
    score += bits.c == 42;

    bits.a = 15;
    score += bits.a == 7;

    return score == 7 ? 42 : score;
}
