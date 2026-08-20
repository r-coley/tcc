typedef enum {
	MODE_OFF = 0,
	MODE_ON = 1,
	MODE_AUTO = 2
} Mode;

int
main(void)
{
	int side_effect = 0;
	int raw = MODE_AUTO;

	switch ((side_effect = 1, (Mode)raw)) {
	case MODE_OFF:
		return 1;
	case MODE_ON:
		return 2;
	case MODE_AUTO:
		return side_effect ? 42 : 3;
	default:
		return 4;
	}
}
