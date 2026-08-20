int
sum_param_star_vla(int values[*])
{
    return values[0] + values[1] + values[2];
}

int
main(void)
{
    int values[3] = { 10, 20, 12 };
    return sum_param_star_vla(values) - 42;
}
