double global_hex = 0x1p+1;
float global_hex_f = 0x1p+2f;

int
main(void)
{
	double local_hex = 0x1.8p+1;
	float local_hex_f = 0x1p-1f;

	if (global_hex != 2.0)
		return 1;
	if (global_hex_f != 4.0f)
		return 2;
	if (local_hex != 3.0)
		return 3;
	if (local_hex_f != 0.5f)
		return 4;
	return 0;
}
