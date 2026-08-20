enum E { EA = 1 };
typedef enum E E;

int main(void)
{
	enum E e = EA;
	return _Generic(e, enum E: 1, E: 2, default: 3);
}
