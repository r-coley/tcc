int
main(void)
{
    int total = 0;
    int once = 0;
    int again = 0;
    int sw = 1;

    if (1) {
        int x = 5;
        total += x;
    } else {
        int dead = 100;
        total += dead;
    }

    if (0) {
        int dead = 100;
        total += dead;
    } else {
        int y = 7;
        total += y;
    }

    while (once == 0) {
        int z = 11;
        total += z;
        once = 1;
    }

    do {
        int q = 13;
        total += q;
        again = 1;
    } while (0);

    for (; sw; ) {
        int f = 17;
        total += f;
        sw = 0;
    }

    switch (1) {
    case 1: {
        int s = 19;
        total += s;
        break;
    }
    default: {
        int dead = 100;
        total += dead;
        break;
    }
    }

    return total;
}
