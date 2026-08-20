enum State { STATE_OFF, STATE_ON = 42 };

int main(void)
{
    enum State value = STATE_ON;
    enum State *ptr = &value;
    return *ptr == STATE_ON ? 0 : 1;
}
