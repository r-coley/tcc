union U {
	int value;
};

int
main(void)
{
	union U u = (union U){};
	return u.value;
}
