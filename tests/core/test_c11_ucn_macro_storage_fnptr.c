#define SUM(na\u00EFve, jalape\u00F1o) ((na\u00EFve) + (jalape\u00F1o))

typedef int (*caf\u00E9_fn)(int);

static int caf\u00E9_static = 20;
int pi\u00F1ata_global = 20;

static int
add_\u03B2(int v)
{
	return v + 2;
}

int
main(void)
{
	caf\u00E9_fn f = add_\u03B2;
	return SUM(caf\u00E9_static, pi\u00F1ata_global) + f(0);
}
