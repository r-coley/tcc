int main() {
    int a[1];
    int y;

    a[0] = 3;
    y = a[0]++;

    return y * 10 + a[0];
}
