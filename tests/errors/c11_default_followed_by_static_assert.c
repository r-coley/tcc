int main(void)
{
    switch (0) {
    default:
        _Static_assert(1, "ok");
        return 42;
    }
}
