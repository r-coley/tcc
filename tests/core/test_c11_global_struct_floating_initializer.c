struct S {
	float x;
	double y;
};

struct S gs = { 1.0f + 2.0f, 1 ? 3.0 : 4.0 };

int
main(void)
{
	if (gs.x != 3.0f)
		return 1;
	if (gs.y != 3.0)
		return 2;
	return 42;
}
