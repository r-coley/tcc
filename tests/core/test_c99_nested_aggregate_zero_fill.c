struct Pair {
	int x;
	int y;
};

struct Outer {
	struct Pair pair;
	int values[3];
};

struct Outer global_a = { {1}, {2} };
struct Outer global_b = { 3, 4, 5 };

int
main(void)
{
	struct Outer local_a = { {6}, {7} };
	struct Outer local_b = { 8, 9, 10 };

	if (global_a.pair.x != 1)
		return 1;
	if (global_a.pair.y != 0)
		return 2;
	if (global_a.values[0] != 2)
		return 3;
	if (global_a.values[1] != 0 || global_a.values[2] != 0)
		return 4;

	if (global_b.pair.x != 3 || global_b.pair.y != 4)
		return 5;
	if (global_b.values[0] != 5)
		return 6;
	if (global_b.values[1] != 0 || global_b.values[2] != 0)
		return 7;

	if (local_a.pair.x != 6)
		return 8;
	if (local_a.pair.y != 0)
		return 9;
	if (local_a.values[0] != 7)
		return 10;
	if (local_a.values[1] != 0 || local_a.values[2] != 0)
		return 11;

	if (local_b.pair.x != 8 || local_b.pair.y != 9)
		return 12;
	if (local_b.values[0] != 10)
		return 13;
	if (local_b.values[1] != 0 || local_b.values[2] != 0)
		return 14;

	return 42;
}
