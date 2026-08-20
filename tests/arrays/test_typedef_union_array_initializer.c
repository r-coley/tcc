typedef union {
	int x;
} U;

int
main(void)
{
	U arr[2] = {{7}, {35}};

	return arr[0].x + arr[1].x;
}
