#define PUSH_ABORT _Pragma("push_macro(\"abort\")")
#define POP_ABORT _Pragma("pop_macro(\"abort\")")

int
main(void)
{
	int fail = 0;

#undef abort
#define abort 11
	if (abort != 11)
		fail++;

#pragma push_macro("abort")
#undef abort
#define abort 22
	if (abort != 22)
		fail++;
#pragma pop_macro("abort")
	if (abort != 11)
		fail++;

	PUSH_ABORT
#undef abort
#define abort 33
	if (abort != 33)
		fail++;
	POP_ABORT
	if (abort != 11)
		fail++;

#pragma push_macro("missing_name")
#define missing_name 77
#pragma pop_macro("missing_name")
#ifdef missing_name
	fail++;
#endif

	return fail;
}
