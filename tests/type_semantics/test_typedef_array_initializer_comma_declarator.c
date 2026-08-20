typedef int Pair[2];

int main(void)
{
	Pair a = {1, 2}, b;

	b[0] = 40;
	b[1] = 2;
	return a[0] + a[1] + b[0] + b[1];
}
