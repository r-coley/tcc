typedef struct {
	int value;
} S;

S g = {};

int
main(void)
{
	return g.value;
}
