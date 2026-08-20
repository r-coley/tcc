int main(void)
{
    int value;
    int *ip;
    void *vp;

    value = 42;
    vp = &value;
    ip = vp;
    return *ip;
}
