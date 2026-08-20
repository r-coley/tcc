/*
 * test_w.c — verifies that -w suppresses compiler warnings.
 *
 * A multi-character character constant triggers a warning from tcc's lexer.
 * Without -w that warning is printed to stderr.
 * With -w the warning is suppressed entirely.
 *
 * Expected exit code: 0
 */
int
main(void)
{
	int x = 'ab'; /* multi-character constant: triggers a warning */
	(void)x;
	return 0;
}
