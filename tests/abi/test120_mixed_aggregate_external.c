typedef struct MixedABI {
	int tag;
	char code;
	void *ptr;
	long value;
} MixedABI;

extern MixedABI make_mixed(int tag, char code, void *ptr, long value);
extern long consume_mixed(MixedABI m, int *expected_ptr);

int
main(void)
{
	int marker = 1234;
	MixedABI m = make_mixed(11, 7, &marker, 24);
	return (int)consume_mixed(m, &marker);
}
