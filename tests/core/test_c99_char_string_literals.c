int
main(void)
{
	char nl;
	char hex;
	char oct;
	char text[6];

	nl = '\n';
	hex = '\x2a';
	oct = '\052';
	text[0] = "he" "llo"[0];
	text[1] = "he" "llo"[1];
	text[2] = "he" "llo"[2];
	text[3] = "he" "llo"[3];
	text[4] = "he" "llo"[4];
	text[5] = '\0';

	if (nl != 10)
		return 1;
	if (hex != 42)
		return 2;
	if (oct != 42)
		return 3;
	if (text[0] != 'h' || text[1] != 'e' || text[2] != 'l' ||
	    text[3] != 'l' || text[4] != 'o' || text[5] != 0)
		return 4;

	return 42;
}
