static long gvals[8];

int main(void) {
    int i = 3;

    gvals[i] = 10;
    gvals[i] = gvals[i] + 32;

    return gvals[i];
}
