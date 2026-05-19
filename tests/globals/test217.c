/* &global_array[i] address computation: ptr_add expects x0=index, x1=base.
   Before fix, base was loaded into x0 then sxtw'd, corrupting the address. */
static int arr[8];

int *get_ptr(int i) {
    return &arr[i];
}

int main() {
    int *p = get_ptr(3);
    *p = 99;
    return arr[3];
}
