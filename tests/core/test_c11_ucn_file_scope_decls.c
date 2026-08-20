#define SUMA(x, y) ((x) + (y))

typedef int caf\u00E9_t;

extern caf\u00E9_t ni\u00F1o_global;
static caf\u00E9_t jalape\u00F1o_static = 20;
caf\u00E9_t ni\u00F1o_global = 20;

static caf\u00E9_t (*pi\u00F1ata_fp)(caf\u00E9_t);

extern caf\u00E9_t funci\u00F3n_prototipo(caf\u00E9_t valor);

static caf\u00E9_t
aux_\u03B3(caf\u00E9_t valor)
{
    return valor + 2;
}

caf\u00E9_t
funci\u00F3n_prototipo(caf\u00E9_t valor)
{
    return SUMA(jalape\u00F1o_static, ni\u00F1o_global) + valor;
}

int
main(void)
{
    pi\u00F1ata_fp = aux_\u03B3;
    return funci\u00F3n_prototipo(pi\u00F1ata_fp(0));
}
