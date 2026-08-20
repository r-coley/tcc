int main(void) {
	register int first = 1, second = 2;
	int *p = &second;
	return first + *p;
}
