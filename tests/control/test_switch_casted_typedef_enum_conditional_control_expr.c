typedef enum {
	MODE_OFF = 0,
	MODE_ON = 1,
	MODE_AUTO = 2
} Mode;

int
main(void)
{
	volatile int choose_on = 1;

	switch (choose_on ? (Mode)MODE_ON : (Mode)MODE_AUTO) {
	case MODE_OFF:
		return 1;
	case MODE_ON:
		return 42;
	case MODE_AUTO:
		return 2;
	default:
		return 3;
	}
}
