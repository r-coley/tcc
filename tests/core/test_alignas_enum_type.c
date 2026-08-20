enum E { EA = 1, EB = 2 };

_Alignas(enum E) char global_enum_aligned;

int
main(void)
{
	_Alignas(enum E) char local_enum_aligned = 0;

	if (((unsigned long)&global_enum_aligned % _Alignof(enum E)) != 0)
		return 1;
	if (((unsigned long)&local_enum_aligned % _Alignof(enum E)) != 0)
		return 2;

	return 42;
}
