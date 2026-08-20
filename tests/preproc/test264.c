#if defined(__aarch64__) || defined(__x86_64__)
int main() { return 42; }
#else
int main() { return 1; }
#endif
