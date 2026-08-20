typedef struct {
	int value;
} S;

int
main(void)
{
	S s = (S){};
	return s.value;
}
