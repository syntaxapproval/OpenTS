#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
inline long filelength(int fd) { struct stat st; return fstat(fd, &st) == 0 ? (long)st.st_size : -1L; }
#define _filelength filelength
#define _unlink unlink
#define _access access
#define _getcwd getcwd
#define _chdir chdir
#define _close close
#define _read read
#define _write write
#define _lseek lseek
#define _open open
#define O_BINARY 0
#define _O_BINARY 0
#define _O_RDONLY O_RDONLY

