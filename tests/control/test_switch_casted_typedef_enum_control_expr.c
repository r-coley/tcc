typedef enum {
	MODE_OFF = 0,
	MODE_ON = 1,
	MODE_AUTO = 2
} Mode;

int
main(void)
{
	int raw = 2;

	switch ((Mode)raw) {
	case MODE_OFF:
		return 1;
	case MODE_ON:
		return 2;
	case MODE_AUTO:
		return 42;
	default:
		return 0;
	}
}
