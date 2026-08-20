struct Point {
    int x;
    int y;
};

int main() {
    return ((struct Point){ .y = 41, .x = 1 }).x +
           ((struct Point){ .y = 41, .x = 1 }).y;
}
