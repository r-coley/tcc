enum Kind { KIND_ZERO = 3, KIND_ONE = 7 };

int main(void)
{
    enum Kind values[2] = { KIND_ZERO, KIND_ONE };
    return values[0] == KIND_ZERO &&
           values[1] == KIND_ONE &&
           sizeof(values) == 2 * sizeof(enum Kind) ? 0 : 1;
}
