typedef int arr3[3];
typedef const int carr3[3];
typedef int matrix2x3[2][3];

int
main(void)
{
	arr3 a = {1, 2, 3};
	carr3 ca = {4, 5, 6};
	matrix2x3 m = {{0}};
	arr3 * const pa = &a;
	carr3 * const pca = &ca;
	matrix2x3 * const pm = &m;
	int total = 0;

	total += _Generic(pa, arr3 *: 10, default: 100);
	total += _Generic(pa, int (*)[3]: 20, default: 100);
	total += _Generic(pca, carr3 *: 30, default: 100);
	total += _Generic(pca, const int (*)[3]: 40, default: 100);
	total += _Generic(pm, matrix2x3 *: 50, default: 100);
	total += _Generic(pm, int (*)[2][3]: 60, default: 100);

	return total == 210 ? 42 : total;
}
