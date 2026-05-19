#define CAT(a,b) a /* left */ ## /* right */ b
#define xy 42

int main() {
    return CAT(x, y);
}
