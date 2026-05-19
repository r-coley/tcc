/* typedef of a forward struct tag */
typedef struct FILE FILE;

struct FILE {
    int dummy;
};

int take_file(FILE *f) {
    return f->dummy;
}

int main(void) {
    struct FILE file;
    file.dummy = 42;
    return take_file(&file);
}
