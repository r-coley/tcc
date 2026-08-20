union U {
	int value;
};

int
main(void)
{
	union U u = {};
	return u.value;
}
