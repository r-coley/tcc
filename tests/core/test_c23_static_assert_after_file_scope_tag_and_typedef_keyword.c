typedef int file_scope_int;

struct FileScopeTagged {
	file_scope_int value;
};

typedef struct FileScopeTagged file_scope_tagged_t;

static_assert(sizeof(file_scope_tagged_t) == sizeof(int), "tagged typedef size");

union FileScopeTaggedUnion {
	long value;
};

typedef union FileScopeTaggedUnion file_scope_tagged_union_t;

static_assert(sizeof(file_scope_tagged_union_t) == sizeof(long), "tagged union size");

int
main(void)
{
	file_scope_tagged_t s;
	file_scope_tagged_union_t u;

	s.value = 42;
	u.value = 0;

	return (s.value == 42 && sizeof(u) == sizeof(long)) ? 42 : 1;
}
