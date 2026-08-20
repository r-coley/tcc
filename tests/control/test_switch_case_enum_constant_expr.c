enum {
    SWITCH_ENUM_BASE = 3
};

int main(void) {
    switch (5) {
    case SWITCH_ENUM_BASE + 2:
        return 42;
    default:
        return 1;
    }
}
