int main() {
    int x;

    x = 0;

    while (x < 50) {
        x = x + 1;

        if (x < 42) {
            continue;
        }

        break;
    }

    return x;
}
