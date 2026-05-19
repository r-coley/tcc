struct Point {
    int x;
    int y;
};

int sum_ptr(struct Point *p) {
    return p->x + p->y;
}

int main() {
    struct Point p = { 1, 41 };

    return sum_ptr(&p);
}
