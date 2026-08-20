int gi[3] = { 10, 20, 30 };
unsigned char gb[4] = { 1, 2, 3, 4 };
unsigned short gw[2] = { 5, 6 };

int sum_arrays(void) {
    return gi[1] + gb[2] + gw[1];
}

int main(void) {
    if (sum_arrays() != 29)
        return 1;
    return 0;
}
