char msg[] = "ABC";
char *pmsg = msg;

int main(void) {
    return pmsg[0] + pmsg[1] + pmsg[2] - 156;
}
