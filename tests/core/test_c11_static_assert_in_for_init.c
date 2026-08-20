int main(void)
{
    int ran = 0;

    for (_Static_assert(1, "ok"); ran == 0; ran = 1) {
        if (ran != 0)
            return 1;
    }

    return ran == 1 ? 42 : 0;
}
