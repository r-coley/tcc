extern __inline __attribute__((__gnu_inline__))
int duplicated_header_inline(void)
{
	return 40;
}

int lib_extern_inline_value(void);

int main(void)
{
	return lib_extern_inline_value();
}
