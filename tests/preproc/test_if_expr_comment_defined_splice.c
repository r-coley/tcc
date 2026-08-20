#define FLAG 42

#if de\
fined(FLAG) && (40 + 2/\
* comment */ == FLAG)
int
main(void)
{
	return FLAG;
}
#else
int
main(void)
{
	return 1;
}
#endif
