int *shared[];
int *shared[];
extern int *shared[];

int
set_shared_pointer(void);

int
main(void)
{
	if (shared[0] != 0)
		return 1;
	return set_shared_pointer();
}
