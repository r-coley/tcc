/*
 * x64 System V ABI smoke test for mixed scalar and 64-bit arguments.
 *
 * This is assembly-smoke coverage for hosts that cannot run i386 binaries.
 * It stresses:
 *
 *   - stack layout for mixed int / pointer / long long arguments
 *   - mixed register/stack argument passing
 *   - 64-bit return value in rax
 *   - carry/borrow propagation for 64-bit add/sub
 *   - caller stack cleanup after calls
 */

long long x64_i64_args_return_mix(int a,
                                  long long b,
                                  int *p,
                                  int c,
                                  long long d)
{
    long long r;

    r = b + d;
    r = r + (long long)a;
    r = r - (long long)c;
    r = r + (long long)*p;

    return r;
}

long long x64_i64_args_return_mix_chain(long long seed)
{
    int local;
    long long r;

    local = 7;

    r = x64_i64_args_return_mix(3,
                                seed,
                                &local,
                                2,
                                0x0000000100000004LL);

    return r - 1;
}

int main(void)
{
    long long r;

    /*
     * Expected runtime value:
     *
     *   seed                      0x00000000fffffffe
     * + d                         0x0000000100000004
     * + a                         3
     * - c                         2
     * + *p                        7
     * - final chain adjustment    1
     * =                           0x0000000200000009
     *
     * Return a compact observable value for future i386 execution runners.
     */
    r = x64_i64_args_return_mix_chain(0x00000000fffffffeLL);

    return (int)((r >> 32) + (r & 0xff));
}
