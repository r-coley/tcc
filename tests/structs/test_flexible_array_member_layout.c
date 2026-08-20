#include <stddef.h>

struct Packet {
	int len;
	char data[];
};

struct WithDouble {
	char tag;
	double values[];
};

int
main(void)
{
	if (sizeof(struct Packet) != offsetof(struct Packet, data))
		return 1;
	if (offsetof(struct Packet, data) != 4)
		return 2;
	if (sizeof(struct WithDouble) != offsetof(struct WithDouble, values))
		return 3;
	if (offsetof(struct WithDouble, values) != 8)
		return 4;
	return 42;
}
