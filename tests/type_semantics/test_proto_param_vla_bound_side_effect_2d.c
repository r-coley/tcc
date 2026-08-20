static int
check_proto_param_vla_bound_side_effect_2d(int n, int values[++n][4]);

static int
check_proto_param_vla_bound_side_effect_2d(int n, int values[++n][4])
{
	return !(n == 3 && values[n - 1][3] == 12);
}

int
main(void)
{
	int values[3][4];
	int i;
	int j;
	int next = 1;

	for (i = 0; i < 3; i = i + 1) {
		for (j = 0; j < 4; j = j + 1) {
			values[i][j] = next;
			next = next + 1;
		}
	}

	return check_proto_param_vla_bound_side_effect_2d(2, values);
}
