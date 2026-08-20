int gi = { 7 };
unsigned char guc = { 300 };
float gf = { 1.5 };
double gd = { 2.25, };
int gni = {{ 9 }};
double gnd = {{ 5.5, }, };

int main(void)
{
	int li = { 8 };
	unsigned char luc = { 301 };
	float lf = { 3.5 };
	double ld = { 4.25, };
	int lni = {{ 10 }};
	double lnd = {{ 6.5, }, };

	if (gi != 7)
		return 1;
	if (guc != 44)
		return 2;
	if (gf != 1.5f)
		return 3;
	if (gd != 2.25)
		return 4;
	if (li != 8)
		return 5;
	if (luc != 45)
		return 6;
	if (lf != 3.5f)
		return 7;
	if (ld != 4.25)
		return 8;
	if (gni != 9)
		return 9;
	if (gnd != 5.5)
		return 10;
	if (lni != 10)
		return 11;
	if (lnd != 6.5)
		return 12;

	return 42;
}
