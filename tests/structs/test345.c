struct Point {
    int x;
    int y;
};

int main() {
    return ((struct Point){ 2, 40 }).x +
           ((struct Point){ 2, 40 }).y;
}
