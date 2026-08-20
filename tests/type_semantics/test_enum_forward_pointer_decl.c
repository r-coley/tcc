enum E;
enum E *global_ptr;

static int
pointer_is_null(enum E *ptr)
{
    return ptr == 0;
}

int main(void)
{
    enum E *local_ptr = 0;
    return pointer_is_null(global_ptr) && pointer_is_null(local_ptr) ? 0 : 1;
}
