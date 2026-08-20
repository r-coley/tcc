inline int
add_one(void)
{
	return 42;
}

inline int add_one(void);

int
main(void)
{
	return add_one();
}
