/* LLDB smoke fixture.
   Keep this deliberately small: the validation script checks that LLDB can
   load the executable, resolve a breakpoint, stop in this function, show
   source/line information, inspect simple parameters/locals, and continue to
   a clean exit. */
static int debug_target(int seed)
{
    int doubled = seed * 2;
    int total = doubled + 5;
    return total;
}

int main(void)
{
    int result = debug_target(7);
    return result == 19 ? 0 : 1;
}
