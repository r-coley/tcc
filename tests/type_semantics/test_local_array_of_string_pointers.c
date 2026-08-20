int main(void)
{
    const char *azOpt[] = { "_ROWID_", "ROWID", "OID" };
    return azOpt[1][0] == 'R' && azOpt[2][0] == 'O' ? 42 : 0;
}
