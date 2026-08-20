int arr[4];

int set(int i, int v) {
    arr[i] = v;
    return 0;
}

int main() {
    set(2, 42);
    return arr[2];
}
