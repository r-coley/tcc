struct FileScopeStruct {
	int value;
};

static_assert(sizeof(struct FileScopeStruct) == sizeof(int), "struct size");

union FileScopeUnion {
	long value;
};

static_assert(sizeof(union FileScopeUnion) == sizeof(long), "union size");

enum FileScopeEnum {
	FILE_SCOPE_ENUM_VALUE = 1
};

static_assert(FILE_SCOPE_ENUM_VALUE == 1, "enum value");

int
file_scope_decl_fn(void);

static_assert(sizeof(int) == 4, "int size");

int
file_scope_decl_fn(void)
{
	return FILE_SCOPE_ENUM_VALUE;
}

int
main(void)
{
	return file_scope_decl_fn() == 1 ? 42 : 1;
}
