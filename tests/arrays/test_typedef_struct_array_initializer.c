typedef struct {
	int x;
} S;

int
main(void)
{
	S arr[2] = {{7}, {35}};

	return arr[0].x + arr[1].x;
}
