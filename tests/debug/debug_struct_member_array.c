struct TccDbgLine {
    int values[3];
};

int debug_struct_member_array_probe(void)
{
    struct TccDbgLine line;

    line.values[0] = 10;
    line.values[1] = 20;
    line.values[2] = 12;

    return line.values[0] + line.values[1] + line.values[2];
}

int main(void)
{
    return debug_struct_member_array_probe() == 42 ? 0 : 1;
}
