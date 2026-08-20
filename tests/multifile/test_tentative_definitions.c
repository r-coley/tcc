int shared;
int shared;
extern int shared;

int bump_shared(void);

int
main(void)
{
	if (shared != 0)
		return 1;
	shared = 40;
	return bump_shared();
}
