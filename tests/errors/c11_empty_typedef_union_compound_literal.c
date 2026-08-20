typedef union {
	int value;
} U;

int
main(void)
{
	U u = (U){};
	return u.value;
}
