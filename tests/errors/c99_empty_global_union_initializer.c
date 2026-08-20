union U {
	int value;
};

union U g = {};

int
main(void)
{
	return g.value;
}
