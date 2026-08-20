static int bool(void) { return 1; }
static int alignas(void) { return 2; }
static int alignof(void) { return 3; }
static int static_assert(void) { return 4; }
static int thread_local(void) { return 5; }
static int true(void) { return 6; }
static int false(void) { return 7; }
static int nullptr(void) { return 8; }
static int nullptr_t(void) { return 6; }

int
main(void)
{
	return bool() +
	       alignas() +
	       alignof() +
	       static_assert() +
	       thread_local() +
	       true() +
	       false() +
	       nullptr() +
	       nullptr_t();
}
