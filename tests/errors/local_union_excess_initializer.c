union U {
	int a;
	int b;
};

int main(void)
{
	union U u = {1, 2};
	return u.a;
}
