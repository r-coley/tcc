#ifndef TCC_STUB_SYS_STAT_H
#define TCC_STUB_SYS_STAT_H

#include <__tcc_stub_common.h>
#include <sys/types.h>
#include <time.h>

struct stat {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	struct timespec st_atimespec;
	struct timespec st_mtimespec;
	struct timespec st_ctimespec;
	struct timespec st_birthtimespec;
	off_t st_size;
	blkcnt_t st_blocks;
	blksize_t st_blksize;
	unsigned int st_flags;
	unsigned int st_gen;
	int st_lspare;
	long long st_qspare[2];
};

int fstat(int fd, struct stat *sb);

#endif
