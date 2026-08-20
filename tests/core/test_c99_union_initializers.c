union U {
	int i;
	unsigned int u;
};

struct Holder {
	int tag;
	union U value;
};

union U global_first = { 42 };
union U global_designated = { .u = 99U };
struct Holder global_holder = { 7, { .u = 123U } };

int
main(void)
{
	union U local_first = { -5 };
	union U local_designated = { .u = 77U };
	struct Holder local_holder = { 8, { .u = 456U } };

	if (global_first.i != 42)
		return 1;
	if (global_designated.u != 99U)
		return 2;
	if (global_holder.tag != 7)
		return 3;
	if (global_holder.value.u != 123U)
		return 4;

	if (local_first.i != -5)
		return 5;
	if (local_designated.u != 77U)
		return 6;
	if (local_holder.tag != 8)
		return 7;
	if (local_holder.value.u != 456U)
		return 8;

	return 42;
}
