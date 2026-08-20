static _Bool
ret_true(void)
{
	return 1;
}

static _Bool
ret_false(void)
{
	return 0;
}

int
main(void)
{
	_Bool (*fp)(void) = ret_true;
	_Bool (*fps[2])(void) = { ret_false, ret_true };
	_Bool (**fpp)(void) = &fp;
	int total = 0;

	if (fp())
		total += 10;
	if (!fps[0]())
		total += 20;
	if ((**fpp)())
		total += 12;

	return total;
}
