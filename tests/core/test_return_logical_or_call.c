int pred(int value, int target)
{
	return value == target;
}

int folded(int value)
{
	return pred(value, 1) ||
	       pred(value, 2);
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
