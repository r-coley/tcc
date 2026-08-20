#include <stddef.h>

long double add(long double a, long double b)
{
	return a + b;
}

typedef long double ld_t;
typedef long double (*ld_binop_t)(long double, long double);

static ld_t
call_binop(ld_binop_t fn, ld_t a, ld_t b)
{
	return fn(a, b);
}

static ld_t (*pick(int which))(ld_t, ld_t)
{
	return which ? add : add;
}

int
main(void)
{
	ld_binop_t fn = add;
	ld_t x = call_binop(fn, (ld_t)20.0, (ld_t)22.0);
	ld_t y = pick(1)((ld_t)10.0, (ld_t)32.0);
	return (x == (ld_t)42.0 && y == (ld_t)42.0) ? 42 : 1;
}
