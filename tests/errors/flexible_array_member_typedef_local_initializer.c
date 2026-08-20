typedef struct {
	int n;
	int values[];
} S;

int
main(void)
{
	S s = { 1, { 2, 3 } };
	return s.n;
}
