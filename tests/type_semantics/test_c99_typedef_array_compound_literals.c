typedef int A3[3];

int
main(void)
{
	A3 *ptr = &(A3){ 10, 20, 30 };
	int sum = 0;

	sum += ((A3){ 1, 2, 3 })[0];
	sum += ((A3){ 1, 2, 3 })[1];
	sum += ((A3){ 1, 2, 3 })[2];
	sum += (*ptr)[0];
	sum += (*ptr)[1];
	sum += (*ptr)[2];

	(*ptr)[1] = 40;
	sum += (*ptr)[1];

	return sum == 106 ? 42 : sum;
}
