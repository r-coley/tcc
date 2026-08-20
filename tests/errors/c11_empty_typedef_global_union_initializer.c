typedef union {
	int value;
} U;

U g = {};

int
main(void)
{
	return g.value;
}
