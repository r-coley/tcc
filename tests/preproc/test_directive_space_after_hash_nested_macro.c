# define BASE_VALUE 10
# define SELECTED_VALUE BASE_VALUE

# if 1
#  define RESULT_VALUE SELECTED_VALUE
# else
#  define RESULT_VALUE 1
# endif

int main(void)
{
    return RESULT_VALUE - 10;
}
