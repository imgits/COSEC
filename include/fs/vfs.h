#ifndef __VFS_H__
#define __VFS_H__

#include <std/sys/stat.h>
#include <std/sys/types.h>

typedef uint16_t uid_t, gid_t;

typedef struct superblock superblock_t;
typedef struct fs_ops fs_ops;
typedef struct inode inode_t;
typedef struct fsdriver fsdriver_t;

#define PINODE_SIZE 128

struct inode {
    dev_t dev_id;         // device
    index_t no;           // ino index
    
    uid_t uid;
    gid_t gid;
    mode_t mode;

    size_t size;
    count_t nlinks;
};

typedef struct pinode {
    inode_t i;
    char info[0];
    char pad[PINODE_SIZE - sizeof(inode_t)];
} pinode_t; // padded inode


#define PFSDRV_SIZE 256


struct fsdriver {
    const char *name;
    fs_ops *ops;
};

typedef struct pfsdriver {
    fsdriver_t fsd;
    char info[0];
    char pad[PFSDRV_SIZE - sizeof(fsdriver_t)];
} pfsdriver_t;  // padded fsdriver

struct fs_ops {
    int (*read_inode)(superblock_t *, inode_t *);
    int (*write_inode)(superblock_t *, inode_t *);
    int (*statfs)(superblock_t *, struct statvfs *);
};

#define PSB_SIZE    512

struct superblock {
    dev_t dev;
    size_t blksz;
    fsdriver_t *fs;
    struct {
        bool dirty :1 ;
        bool ro :1 ;
    } flags;
};

typedef struct psuperblock {
    superblock_t sb;
    char info[0];
    char pad[PSB_SIZE - sizeof(superblock_t)];
} psuperblock_t; // padded superblock


typedef  struct mount_opts_t  mount_opts_t;

err_t vfs_mount(const char *source, const char *target, const mount_opts_t *opts);
err_t vfs_mkdir(const char *path, mode_t mode);

void print_ls(const char *path);
void print_mount(void);
void vfs_shell(const char *);

void vfs_setup(void);

#endif // __VFS_H__
