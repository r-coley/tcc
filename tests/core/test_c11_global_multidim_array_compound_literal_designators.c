static int (*g_rows)[2] =
    (int[2][2]){ [1] = { [0] = 30 }, [0] = { [1] = 12 } };
static int (*g_full)[2][2] =
    &(int[2][2]){ [1] = { [0] = 30 }, [0] = { [1] = 12 } };
static int g_val =
    ((int[2][2]){ [1] = { [0] = 30 }, [0] = { [1] = 12 } })[1][0];

int
main(void)
{
    if (g_rows[0][0] != 0 || g_rows[0][1] != 12)
        return 1;
    if (g_rows[1][0] != 30 || g_rows[1][1] != 0)
        return 2;
    if ((*g_full)[1][0] != 30 || (*g_full)[0][1] != 12)
        return 3;
    if (g_val != 30)
        return 4;
    return g_rows[0][1] + g_rows[1][0] == 42 ? 42 : 5;
}
