typedef unsigned long size_t;

typedef struct CollSeq CollSeq;
typedef struct KeyInfo KeyInfo;

struct KeyInfo {
	int nRef;
	int nKeyField;
	CollSeq *aColl[1];
};

#define SZ_KEYINFO(N) ((size_t)(&((KeyInfo*)0)->aColl) + (N) * sizeof(CollSeq *))

struct Holder {
	unsigned char keyinfoSpace[SZ_KEYINFO(3)];
};

int
main(void)
{
	int fail = 0;

	if (SZ_KEYINFO(0) != 8) fail++;
	if (SZ_KEYINFO(3) != 32) fail++;
	if (sizeof(struct Holder) != 32) fail++;

	return fail;
}
