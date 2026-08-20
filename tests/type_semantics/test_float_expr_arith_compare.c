int main(void)
{
	double d = 1.5 + 2.5;
	float f = 6.0f / 2.0f;
	double mixed = 2 + 2.5;
	int score = 0;

	if (d == 4.0)
		score += 10;
	if (f == 3.0f)
		score += 10;
	if (mixed == 4.5)
		score += 10;
	if ((2.0 < 3.0) && (4.0 >= 4.0))
		score += 12;
	return score;
}
