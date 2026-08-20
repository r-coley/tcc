extern int external_value;

int *get_external_ptr(void) {
    return &external_value;
}

int main(void) {
    int *p = get_external_ptr();
    return p != 0;
}
