#define PUSH_ABORT _Pragma("push_macro(\"ab" "ort\")")
#define POP_ABORT _Pragma("pop_macro(\"ab" "ort\")")

int
main(void)
{
	int fail = 0;

#undef abort
#define abort 11
	PUSH_ABORT
#undef abort
#define abort 22
	if (abort != 22)
		fail++;
	POP_ABORT
	if (abort != 11)
		fail++;

	return fail;
}
