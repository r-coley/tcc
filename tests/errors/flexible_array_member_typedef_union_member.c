typedef struct {
	int n;
	int data[];
} Flex;

union U {
	Flex field;
};
