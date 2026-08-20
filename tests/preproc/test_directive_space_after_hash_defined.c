# define FEATURE_ENABLED 1

# if defined(FEATURE_ENABLED)
int selected_defined(void) { return 17; }
# else
int selected_defined(void) { return 3; }
# endif

# if !defined(MISSING_FEATURE)
int selected_not_defined(void) { return 25; }
# else
int selected_not_defined(void) { return 5; }
# endif

int main(void)
{
    return selected_defined() + selected_not_defined() - 42;
}
