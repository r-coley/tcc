typedef struct sqlite3_mutex sqlite3_mutex;
typedef struct sqlite3_mutex_methods sqlite3_mutex_methods;

struct sqlite3_mutex_methods {
	int (*xMutexInit)(void);
	int (*xMutexEnd)(void);
	sqlite3_mutex *(*xMutexAlloc)(int);
	void (*xMutexFree)(sqlite3_mutex *);
	void (*xMutexEnter)(sqlite3_mutex *);
	int (*xMutexTry)(sqlite3_mutex *);
	void (*xMutexLeave)(sqlite3_mutex *);
	int (*xMutexHeld)(sqlite3_mutex *);
	int (*xMutexNotheld)(sqlite3_mutex *);
};

static int
mutex_init(void)
{
	return 1;
}

static int
mutex_end(void)
{
	return 2;
}

static sqlite3_mutex *
mutex_alloc(int id)
{
	(void)id;
	return 0;
}

static void
mutex_void(sqlite3_mutex *p)
{
	(void)p;
}

static int
mutex_probe(sqlite3_mutex *p)
{
	(void)p;
	return 42;
}

static const sqlite3_mutex_methods methods = {
	mutex_init,
	mutex_end,
	mutex_alloc,
	mutex_void,
	mutex_void,
	mutex_probe,
	mutex_void,
	mutex_probe,
	mutex_probe,
};

int
main(void)
{
	if (sizeof(sqlite3_mutex_methods) != sizeof(struct sqlite3_mutex_methods))
		return 1;
	if (sizeof(sqlite3_mutex_methods) != 9 * (int)sizeof(void *))
		return 2;
	if (methods.xMutexInit() != 1)
		return 3;
	if (methods.xMutexEnd() != 2)
		return 4;
	if (methods.xMutexAlloc(0) != 0)
		return 5;
	if (methods.xMutexTry(0) != 42)
		return 6;
	if (methods.xMutexHeld(0) != 42)
		return 7;
	if (methods.xMutexNotheld(0) != 42)
		return 8;
	return 42;
}
