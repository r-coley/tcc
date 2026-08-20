struct Flex {
	int n;
	int data[];
};

union U {
	struct Flex field;
};
