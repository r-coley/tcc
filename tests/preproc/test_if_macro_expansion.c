# define BASE_VALUE 10
# define SELECTED_VALUE BASE_VALUE
# define ADD_ONE(x) ((x) + 1)
# define COMPUTED_VALUE ADD_ONE(BASE_VALUE)

# if SELECTED_VALUE == 10
int object_like_selected(void) { return 11; }
# else
int object_like_selected(void) { return 1; }
# endif

# if COMPUTED_VALUE == 11
int function_like_selected(void) { return 31; }
# else
int function_like_selected(void) { return 3; }
# endif

int main(void)
{
    return object_like_selected() + function_like_selected() - 42;
}
