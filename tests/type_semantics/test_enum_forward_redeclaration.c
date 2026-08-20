enum E;
enum E;
enum E *global_ptr;

int main(void)
{
    enum E;
    enum E { VALUE = 42 };
    enum E *local_ptr = 0;
    return global_ptr == 0 && local_ptr == 0 && VALUE == 42 ? 0 : 1;
}
