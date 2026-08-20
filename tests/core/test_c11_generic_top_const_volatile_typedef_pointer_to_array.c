typedef int arr3[3];
typedef const int carr3[3];
typedef int matrix2x3[2][3];

typedef arr3 *arr3p;
typedef carr3 *carr3p;
typedef matrix2x3 *matrix2x3p;

int
main(void)
{
	arr3 a = {1, 2, 3};
	carr3 ca = {4, 5, 6};
	matrix2x3 m = {{0}};
	arr3p const volatile pa = &a;
	carr3p const volatile pca = &ca;
	matrix2x3p const volatile pm = &m;
	int total = 0;

	total += _Generic(pa, arr3p: 10, default: 100);
	total += _Generic(pa, int (*)[3]: 20, default: 100);
	total += _Generic(pca, carr3p: 30, default: 100);
	total += _Generic(pca, const int (*)[3]: 40, default: 100);
	total += _Generic(pm, matrix2x3p: 50, default: 100);
	total += _Generic(pm, int (*)[2][3]: 60, default: 100);

	return total == 210 ? 42 : total;
}
