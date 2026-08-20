/* Regression: compound statement directly after a case label must parse correctly.
 * Stage1 tcc failed with "Expected expression factor" when the first token after
 * case X: was { — the case body loop was consuming { manually instead of
 * delegating to parse_statement() which correctly handles compound blocks.
 */
static int classify_escape(int c)
{
    int result = 0;
    switch (c) {
    case 'n': case 'r': case 't': case 'f': case 'v':
    case 'a': case 'b': {
        /* whitespace/control escapes: block with local var */
        int kind = 1;
        result = kind;
        break;
    }
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7': {
        /* octal digit */
        int kind = 2;
        result = kind;
        break;
    }
    case 'x': case 'X': {
        /* hex prefix */
        int kind = 3;
        result = kind;
        break;
    }
    default:
        result = 0;
        break;
    }
    return result;
}

int main(void)
{
    if (classify_escape('n') != 1) return 1;
    if (classify_escape('t') != 1) return 1;
    if (classify_escape('a') != 1) return 1;
    if (classify_escape('3') != 2) return 1;
    if (classify_escape('7') != 2) return 1;
    if (classify_escape('0') != 2) return 1;
    if (classify_escape('x') != 3) return 1;
    if (classify_escape('X') != 3) return 1;
    if (classify_escape('z') != 0) return 1;
    return 42;
}
