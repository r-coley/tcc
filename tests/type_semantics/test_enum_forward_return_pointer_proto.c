enum E;

static enum E *
return_null(void)
{
	return 0;
}

int
main(void)
{
	return return_null() == 0 ? 0 : 1;
}
