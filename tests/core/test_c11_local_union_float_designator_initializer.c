union U {
	float f;
	int i;
};

int
main(void)
{
	union U lu = { .f = 1.0f + 2.0f };

	return lu.f == 3.0f ? 42 : 1;
}
