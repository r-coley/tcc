int shared_inline_value_with_proto_after_type(void);
int call_inline_value_with_proto_after_type(void);

int
main(void)
{
	return call_inline_value_with_proto_after_type() + 2;
}
