/* Regression/coverage for 8-byte local load-after-store forwarding.
 *
 * This keeps the use deliberately simple: pointer-sized local store followed
 * by a load of the same local.  With -O1 the adjacent store/load forwarding
 * pass may rewrite the reload, and this test catches unsafe 32-bit or
 * size-mismatched forwarding of pointer-sized locals.
 */
int main(void) {
    char *p = "abcd";
    char *q;

    q = p;

    if (q != p)
        return 1;

    return 42;
}
