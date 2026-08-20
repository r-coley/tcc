#ifndef TCC_STUB_SYS_TYPES_H
#define TCC_STUB_SYS_TYPES_H

/*
 * Keep these definitions ABI-aligned with cc/include/sys/types.h.
 * This file is not on the normal installed user include path, but if it is
 * seen during a bootstrap/self-host path we do not want a second, different
 * sys/types model for dev_t/ino_t/off_t and friends.
 */
typedef long               ssize_t;
typedef unsigned long      size_t;
typedef int                pid_t;
typedef unsigned int       uid_t;
typedef unsigned int       gid_t;
typedef unsigned short     mode_t;
typedef unsigned int       dev_t;
typedef unsigned long long ino_t;
typedef unsigned short     nlink_t;
typedef long long          off_t;
typedef int                blksize_t;
typedef long long          blkcnt_t;
typedef long               time_t;
typedef long               clock_t;

#endif
