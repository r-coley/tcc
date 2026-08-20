int
sum_proto_param_star_vla(int values[*]);

int
sum_proto_param_star_vla(int values[*])
{
    return values[0] + values[1] + values[2];
}

int
main(void)
{
    int values[3] = { 11, 20, 11 };
    return sum_proto_param_star_vla(values) - 42;
}
