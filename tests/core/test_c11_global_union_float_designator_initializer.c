union U {
	float f;
	int i;
};

union U gu = { .f = 1.0f + 2.0f };

int
main(void)
{
	return gu.f == 3.0f ? 42 : 1;
}
