#ifdef __TCC_VERSION__
int main() { return __TCC_VERSION__ - 41; }
#else
int main() { return 1; }
#endif
