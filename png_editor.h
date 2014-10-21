#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

typedef struct {
    unsigned int length;
    char type[5];
    unsigned char *data;
    unsigned char *crc;
} Chunk;
