enum { TARGET_ONE = 1, TARGET_TWO = 2 };

int pred(int value, int target)
{
	return value == target;
}

int folded(int value)
{
	return pred(value, TARGET_ONE) ||
	       pred(value, TARGET_TWO);
}

int main(void)
{
	if (!folded(1))
		return 1;
	if (!folded(2))
		return 2;
	if (folded(3))
		return 3;
	return 42;
}
