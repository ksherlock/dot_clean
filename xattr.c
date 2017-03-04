
#include "xattr.h"

#if defined(__APPLE__)
#include <sys/xattr.h>
#endif

#if defined(__linux__)
#include <sys/xattr.h>
#endif

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/extattr.h>
#endif

#if defined(_AIX)
#include <sys/ea.h>
#endif


/*
 * extended attributes functions.
 */
#if defined(__APPLE__)
ssize_t size_xattr(int fd, const char *xattr) {
	return fgetxattr(fd, xattr, NULL, 0, 0, 0);
}

ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
	return fgetxattr(fd, xattr, buffer, size, 0, 0);
}

ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
	if (fsetxattr(fd, xattr, buffer, size, 0, 0) < 0) return -1;
	return size;
}

int remove_xattr(int fd, const char *xattr) {
	return fremovexattr(fd, xattr, 0);
}

#elif defined(__linux__) 
ssize_t size_xattr(int fd, const char *xattr) {
	return fgetxattr(fd, xattr, NULL, 0);
}

ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
	return fgetxattr(fd, xattr, buffer, size);
}

ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
	if (fsetxattr(fd, xattr, buffer, size, 0) < 0) return -1;
	return size;
}

int remove_xattr(int fd, const char *xattr) {
	return fremovexattr(fd, xattr);
}

#elif defined(__FreeBSD__)
ssize_t size_xattr(int fd, const char *xattr) {
	return extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, xattr, NULL, 0);
}

ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
	return extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, xattr, buffer, size);
}

ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
	return extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, xattr, buffer, size);
}

int remove_xattr(int fd, const char *xattr) {
	return extattr_delete_fd(fd, EXTATTR_NAMESPACE_USER, xattr);
}

#elif defined(_AIX)
ssize_t size_xattr(int fd, const char *xattr) {
	/*
	struct stat64x st;
	if (fstatea(fd, xattr, &st) < 0) return -1;
	return st.st_size;
	*/
	return fgetea(fd, xattr, NULL, 0);
}

ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
	return fgetea(fd, xattr, buffer, size);
}

ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
	if (fsetea(fd, xattr, buffer, size, 0) < 0) return -1;
	return size;
}

int remove_xattr(int fd, const char *xattr) {
	return fremoveea(fd, xattr);
}

#endif
