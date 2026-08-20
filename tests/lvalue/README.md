# Lvalue tests

This directory is reserved for the next phase.

Planned tests:

```c
int main() {
    int a[2];
    a[0] = 5;
    a[0] += 3;
    return a[0];
}
```

```c
int main() {
    int x;
    int *p;
    x = 4;
    p = &x;
    *p += 2;
    return x;
}
```

These are not enabled in v65 because v65 is only the non-invasive scaffold.
