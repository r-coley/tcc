struct Mixed {
    char c;
    int n;
};

int main() {
    return ((struct Mixed){ .n = 40, .c = 2 }).c +
           ((struct Mixed){ .n = 40, .c = 2 }).n;
}
