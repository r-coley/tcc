struct Inner {
	int value;
};

typedef struct Inner Inner;

struct S {
	Inner x : 1;
};
