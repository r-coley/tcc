enum E;

int pointer_is_null(enum E *ptr);

enum E { VALUE = 42 };

int
pointer_is_null(enum E *ptr)
{
	return ptr == 0;
}

int
main(void)
{
	enum E *local_ptr = 0;
	return pointer_is_null(local_ptr) ? 0 : 1;
}
