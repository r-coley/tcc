/* Verify macros with more than 16 parameters are accepted.
 * Previously rejected with "Too many macro parameters". */
#define SUM17(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q) \
    ((a)+(b)+(c)+(d)+(e)+(f)+(g)+(h)+(i)+(j)+(k)+(l)+(m)+(n)+(o)+(p)+(q))

int main(void) {
    return SUM17(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1) - 17;
}
