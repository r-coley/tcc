struct S { int x; };
typedef struct S S;

union U { int x; };
typedef union U U;

enum E { EA = 1 };
typedef enum E E;

int main(void)
{
	struct S s = {1};
	union U u = {2};
	enum E e = EA;

	if (_Generic(s, struct S: 10, default: 1) != 10)
		return 1;
	if (_Generic(s, S: 20, default: 1) != 20)
		return 2;
	if (_Generic(u, union U: 30, default: 1) != 30)
		return 3;
	if (_Generic(u, U: 40, default: 1) != 40)
		return 4;
	if (_Generic(e, enum E: 50, default: 1) != 50)
		return 5;
	if (_Generic(e, E: 60, default: 1) != 60)
		return 6;
	return 42;
}
