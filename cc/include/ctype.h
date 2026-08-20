#ifndef TCC_STUB_CTYPE_H
#define TCC_STUB_CTYPE_H

static int isdigit(int c) { return c >= '0' && c <= '9'; }
static int isxdigit(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int isalnum(int c) { return isdigit(c) || isalpha(c); }
static int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
static int isupper(int c) { return c >= 'A' && c <= 'Z'; }
static int islower(int c) { return c >= 'a' && c <= 'z'; }
static int isprint(int c) { return c >= 0x20 && c < 0x7f; }
static int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
static int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

#endif
