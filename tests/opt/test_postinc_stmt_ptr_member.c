/* Regression for dead postfix-statement updates through saved indirect lvalues. */

struct Cache {
    long pad[3];
    long nRefSum;
};

struct PgHdr {
    long pad[3];
    long nRef;
    struct Cache *pCache;
};

static void bump(struct PgHdr *p) {
    p->nRef++;
    p->pCache->nRefSum++;
}

int main(void) {
    struct Cache c = {{0, 0, 0}, 0};
    struct PgHdr p = {{0, 0, 0}, 0, &c};

    bump(&p);

    if (p.nRef != 1)
        return 1;
    if (c.nRefSum != 1)
        return 2;

    return 42;
}
