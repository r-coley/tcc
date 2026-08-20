extern int shared;

int
bump_shared(void)
{
	shared = shared + 2;
	return shared;
}
