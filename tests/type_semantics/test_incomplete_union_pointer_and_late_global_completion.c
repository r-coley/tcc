union U;
union U obj;
union U *p;
union U { int x; };

int main(void)
{
	union U *q = p;
	obj.x = 42;
	return q == p ? obj.x : 1;
}
