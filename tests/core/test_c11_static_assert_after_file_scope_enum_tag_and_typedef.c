enum FileScopeEnumTagged {
	FILE_SCOPE_ENUM_TAGGED_VALUE = 42
};

typedef enum FileScopeEnumTagged file_scope_enum_tagged_t;

_Static_assert(FILE_SCOPE_ENUM_TAGGED_VALUE == 42, "enum value");
_Static_assert(sizeof(file_scope_enum_tagged_t) == sizeof(int), "enum typedef size");

int
main(void)
{
	file_scope_enum_tagged_t value = FILE_SCOPE_ENUM_TAGGED_VALUE;

	return value;
}
