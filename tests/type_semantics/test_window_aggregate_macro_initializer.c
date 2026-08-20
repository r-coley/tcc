typedef struct Ctx Ctx;
typedef struct Val Val;

typedef void (*StepFn)(Ctx *, int, Val **);
typedef void (*FinalFn)(Ctx *);
typedef void (*ValueFn)(Ctx *);
typedef void (*InverseFn)(Ctx *, int, Val **);

struct FuncDef {
	int nArg;
	void *pUserData;
	StepFn xStep;
	FinalFn xFinalize;
	ValueFn xValue;
	InverseFn xInverse;
};

#define INT_TO_PTR(i) ((void *)(long)(i))
#define WAGGREGATE(nArg, arg, xStep, xFinal, xValue, xInverse) \
	{ nArg, INT_TO_PTR(arg), xStep, xFinal, xValue, xInverse }

static void
countStep(Ctx *ctx, int argc, Val **argv)
{
	(void)ctx;
	(void)argc;
	(void)argv;
}

static void
countFinalize(Ctx *ctx)
{
	(void)ctx;
}

static void
countInverse(Ctx *ctx, int argc, Val **argv)
{
	(void)ctx;
	(void)argc;
	(void)argv;
}

static struct FuncDef funcs[] = {
	WAGGREGATE(0, 0, countStep, countFinalize, countFinalize, countInverse),
	WAGGREGATE(1, 0, countStep, countFinalize, countFinalize, countInverse),
};

int
main(void)
{
	return funcs[0].xStep == 0 ||
	       funcs[0].xFinalize == 0 ||
	       funcs[0].xValue == 0 ||
	       funcs[0].xInverse == 0;
}
