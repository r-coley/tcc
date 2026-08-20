/* Returning char from function called through pointer chain */
char id_char(char c) { return c; }
int main() {
    char c = id_char(42);
    return c;
}
