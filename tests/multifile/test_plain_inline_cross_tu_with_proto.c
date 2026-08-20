int shared_inline_value_with_proto(void);
int call_inline_value_with_proto(void);

int
main(void)
{
	return call_inline_value_with_proto() + 2;
}
