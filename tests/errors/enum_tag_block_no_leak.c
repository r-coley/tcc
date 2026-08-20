int
main(void)
{
	{
		enum E { A = 3 };
	}

	enum E e;
	(void)e;
	return 0;
}
