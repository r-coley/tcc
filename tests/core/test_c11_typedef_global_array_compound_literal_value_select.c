typedef int A3[3];
typedef int A22[2][2];

static int g1 = ((A3){ 4, 5, 6 })[1];
static int g2 = ((A22){ { 1, 2 }, { 3, 4 } })[1][0];

int main(void)
{
    return (g1 == 5 && g2 == 3) ? 42 : 1;
}
