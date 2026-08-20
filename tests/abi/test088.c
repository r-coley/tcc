#include <unistd.h>
int main() {
    return write(1, "", 0);
}
