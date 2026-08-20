extern int ext_value;

int *get_ext_ptr(void) {
    return &ext_value;
}

int main(void) {
    int *p;
    p = get_ext_ptr();
    return p != 0;
}
