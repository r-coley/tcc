# define VALUE 42

# if VALUE == 42
int answer(void) { return VALUE; }
# else
int answer(void) { return 0; }
# endif

int main(void)
{
    return answer();
}
