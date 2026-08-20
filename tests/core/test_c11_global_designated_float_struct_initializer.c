struct S {
	int pad;
	float x;
	double y;
};

struct S gs = { .x = 1.0f + 2.0f, .y = 1 ? 3.0 : 4.0 };

int
main(void)
{
	if (gs.pad != 0)
		return 1;
	if (gs.x != 3.0f)
		return 2;
	if (gs.y != 3.0)
		return 3;
	return 42;
}
