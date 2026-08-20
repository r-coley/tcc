struct Point {
	int x;
	int y;
};

int main(void)
{
	struct Point *p = &(struct Point){1, 41};
	struct Point *q = &((struct Point){.y = 40, .x = 2});

	if (p->x + p->y != 42)
		return 1;
	if (q->x + q->y != 42)
		return 2;

	p->x = 20;
	q->y = 22;
	if (p->x + q->y != 42)
		return 3;

	return 42;
}
