long double x;

int main(void)
{
	return sizeof(x) == sizeof(double) ? 42 : 1;
}
