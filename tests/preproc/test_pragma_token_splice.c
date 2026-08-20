#define PUSH_ABORT _Pra\
gma("push_macro(\"abort\")")
#define POP_ABORT _Pragma("pop_macro(\"abort\")")

int
main(void)
{
	int fail = 0;

#undef abort
#define abort 11
	PUSH_ABORT
#undef abort
#define abort 22
	POP_ABORT

	if (abort != 11)
		fail++;

	return fail;
}
