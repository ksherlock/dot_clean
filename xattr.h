#ifndef xattr_h
#define xattr_h

#include <stddef.h>
#include <sys/types.h>

#include <errno.h>

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#ifdef __cplusplus
extern "C" {
#endif

ssize_t size_xattr(int fd, const char *xattr);
ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size);
ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size);
int remove_xattr(int fd, const char *xattr);


#ifdef __cplusplus
}
#endif

#endif
