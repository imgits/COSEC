#ifndef __BLOCK_DEV_H__
#define __BLOCK_DEV_H__

#include <std/sys/types.h>
#include <dev/table.h>

typedef struct block_ops {
    int (*read_block)(block_dev_t*, index_t, char *);
    int (*write_block)(block_dev_t*, index_t, const char *);
} block_ops;

#endif // __BLOCK_DEV_H__
