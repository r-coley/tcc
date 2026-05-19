int x = 40;
int *p = &x;

int main() {
    *p = *p + 2;
    return x;
}
