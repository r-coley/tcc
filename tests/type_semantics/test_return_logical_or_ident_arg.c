enum K { K_A, K_B };

int is_kind(int value, int kind)
{
    return value == kind;
}

int match_kind(int value)
{
    return is_kind(value, K_A) ||
           is_kind(value, K_B);
}

int main(void)
{
    return match_kind(K_B) ? 0 : 1;
}
