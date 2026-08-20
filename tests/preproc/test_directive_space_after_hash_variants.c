# define VALUE 42
# define OTHER 7

# ifdef VALUE
#  ifndef MISSING_VALUE
#   if VALUE == 42
#    undef OTHER
#    ifndef OTHER
int selected(void) { return VALUE; }
#    else
int selected(void) { return 1; }
#    endif
#   else
int selected(void) { return 2; }
#   endif
#  else
int selected(void) { return 3; }
#  endif
# else
int selected(void) { return 4; }
# endif

int main(void)
{
    return selected();
}
