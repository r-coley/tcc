bool global_flag = true;

int
main(void)
{
	bool local_flag = false;
	if (!global_flag)
		return 1;
	if (local_flag)
		return 1;
	return 42;
}
