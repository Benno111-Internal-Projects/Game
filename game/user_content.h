#ifndef DORF_USER_CONTENT_H
#define DORF_USER_CONTENT_H

#include <stdbool.h>
#include <stddef.h>

bool UserContentPath(char *path, size_t capacity, const char *filename, bool create);

#endif
