typedef int arr3[3];
typedef const int carr3[3];
typedef int matrix2x3[2][3];

#define TYPE_PTR_ARR3(x) _Generic((x), arr3 *: 1, default: 99)
#define TYPE_PTR_CARR3(x) _Generic((x), carr3 *: 2, default: 99)
#define TYPE_PTR_MATRIX2X3(x) _Generic((x), matrix2x3 *: 3, default: 99)
#define TYPE_INT_PTR(x) _Generic((x), int *: 4, default: 99)
#define TYPE_CONST_INT_PTR(x) _Generic((x), const int *: 5, default: 99)
#define TYPE_PTR_DIRECT_ARR3(x) _Generic((x), int (*)[3]: 6, default: 99)

int
main(void)
{
	arr3 a = {1, 2, 3};
	carr3 ca = {4, 5, 6};
	matrix2x3 m = {{0}};

	if (TYPE_PTR_ARR3(&a) != 1)
		return 1;
	if (TYPE_PTR_CARR3(&ca) != 2)
		return 2;
	if (TYPE_PTR_MATRIX2X3(&m) != 3)
		return 3;
	if (TYPE_PTR_DIRECT_ARR3(&a) != 6)
		return 4;

	if (TYPE_INT_PTR(a) != 4)
		return 5;
	if (TYPE_CONST_INT_PTR(ca) != 5)
		return 6;
	if (TYPE_PTR_ARR3(m) != 1)
		return 7;
	if (TYPE_INT_PTR(m[0]) != 4)
		return 8;

	return 42;
}
