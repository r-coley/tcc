typedef struct MixedABI {
	int tag;
	char code;
	void *ptr;
	long value;
} MixedABI;

MixedABI
make_mixed(int tag, char code, void *ptr, long value)
{
	MixedABI m;
	m.tag = tag;
	m.code = code;
	m.ptr = ptr;
	m.value = value;
	return m;
}

long
consume_mixed(MixedABI m, int *expected_ptr)
{
	long sum = 0;

	if (m.tag == 11)
		sum += 5;
	if (m.code == 7)
		sum += 6;
	if (m.ptr == expected_ptr)
		sum += 7;
	if (m.value == 24)
		sum += 24;

	return sum;
}
