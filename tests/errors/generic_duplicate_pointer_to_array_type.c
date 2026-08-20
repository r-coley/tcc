typedef int arr3[3];
typedef int matrix_t[2][3];

int
main(void)
{
    int a[3];
    int matrix[2][3];

    return _Generic(&a, int (*)[3]: 1, arr3 *: 2, default: 3) +
           _Generic(&matrix, int (*)[2][3]: 1, matrix_t *: 2, default: 3);
}
