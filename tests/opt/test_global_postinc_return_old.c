static int g_counter;

static struct {
    int next_label;
} g_state;

static int
mix4(int a, int b, int c, int d)
{
    return a + b + c + d;
}

static int
check_global_postinc_old_value(void)
{
    int label = g_counter++;

    if (label != 0)
        return 0;
    if (g_counter != 1)
        return 0;
    return mix4(1, 2, label, 4) == 7;
}

static int
check_global_member_postinc_old_value(void)
{
    int label = g_state.next_label++;

    if (label != 0)
        return 0;
    if (g_state.next_label != 1)
        return 0;
    return mix4(1, 2, label, 4) == 7;
}

int
main(void)
{
    if (!check_global_postinc_old_value())
        return 1;
    if (!check_global_member_postinc_old_value())
        return 2;
    return 0;
}
