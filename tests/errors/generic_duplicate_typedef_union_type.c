union U { int x; };
typedef union U U;

int main(void)
{
	union U u = {1};
	return _Generic(u, union U: 1, U: 2, default: 3);
}
