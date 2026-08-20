union U {
	int x;
};

struct S {
	union U field : 1;
};
