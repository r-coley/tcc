int main(void) {
	int x = 0;
	return _Generic(x, _Complex double: 1, default: 0);
}
