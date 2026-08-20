typedef union {
	int value;
} U;

int
main(void)
{
	U u = {};
	return u.value;
}
