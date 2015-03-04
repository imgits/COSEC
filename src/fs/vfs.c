#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/errno.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#include <dev/devices.h>
#include <conf.h>
#include <log.h>

#define FS_SEP  '/'

typedef uint16_t uid_t, gid_t;
typedef size_t inode_t;

typedef struct superblock   mountnode;
typedef struct fs_ops       fs_ops;
typedef struct inode        inode;
typedef struct fsdriver     fsdriver;

typedef struct mount_opts_t  mount_opts_t;

err_t vfs_mount(dev_t source, const char *target, const mount_opts_t *opts);
err_t vfs_mkdir(const char *path, mode_t mode);



struct superblock {
    dev_t dev;
    fsdriver *fs;

    size_t blksz;
    struct {
        bool dirty :1 ;
        bool ro :1 ;
    } flags;

    void *data;                   /* superblock-specific info */

    const char *relmntpath;       /* path relative to the parent mountpoint */
    uint32_t mntpath_hash;        /* hash of this relmntpath */
    struct superblock *brother;   /* the next superblock in the list of childs of parent */
    struct superblock *parent;
    struct superblock *childs;    /* list of this superblock child blocks */
};

struct superblock *theRootMnt = NULL;

#define N_DIRECT_BLOCKS  12
#define MAX_SHORT_SYMLINK_SIZE   60

struct inode {
    index_t no;         // ino index
    mode_t mode;        // inode type + unix permissions
    count_t nlinks;
    void *inode_data;

    union {
        struct {
            size_t block_coout; // how many blocks its data takes
            size_t directblock[ N_DIRECT_BLOCKS ];  // numbers of first DIRECT_BLOCKS blocks
            size_t indir1st_block;
            size_t indir2nd_block;
            size_t indir3rd_block;
        } reg;
        struct { } dir;
        struct {
            majdev_t maj;
            mindev_t min;
        } dev;
        struct {
            char short_symlink[ MAX_SHORT_SYMLINK_SIZE ];
            const char *long_symlink;
        } symlink;
        struct { } socket;
        struct { } namedpipe;
    } as;
};


struct mount_opts_t {
    uint fs_id;
    bool readonly:1;
};

struct fsdriver {
    const char *name;
    uint fs_id;
    fs_ops *ops;

    struct {
        fsdriver *prev, *next;
    } lst;
};



struct fs_ops {
    int (*read_superblock)(mountnode *sb);
    int (*make_directory)(mountnode *sb, inode_t *ino, const char *path, mode_t mode);
    int (*get_direntry)(mountnode *sb, inode_t ino, void **iter, struct dirent *dir);
    int (*make_inode)(mountnode *sb, inode_t *ino, mode_t mode, void *info);
    int (*lookup_inode)(mountnode *sb, inode_t *ino, const char *path);
    int (*link_inode)(mountnode *sb, inode_t ino, const char *path);
    int (*unlink_inode)(mountnode *sb, inode_t ino);
};

fsdriver * theFileSystems = NULL;

void vfs_register_filesystem(fsdriver *fs) {
    if (!theFileSystems) {
        theFileSystems = fs;
        fs->lst.next = fs->lst.prev = fs;
        return;
    }

    /* insert just before theFileSystems in circular list */
    fsdriver *lastfs = theFileSystems->lst.prev;
    fs->lst.next = theFileSystems;
    fs->lst.prev = lastfs;
    lastfs->lst.next = fs;
    theFileSystems->lst.prev = fs;
};

fsdriver *vfs_fs_by_id(uint fs_id) {
    fsdriver *fs = theFileSystems;
    if (!fs) return NULL;

    do {
        if (fs->fs_id == fs_id)
            return fs;
    } while ((fs = fs->lst.next) != theFileSystems);
    return NULL;
}


/*
 *  Sysfs
 */
#if 0
#define SYSFS_ID  0x00535953

static int sysfs_read_superblock(struct superblock *sb);
static int sysfs_make_directory(struct superblock *sb, const char *path, mode_t mode);
static int sysfs_get_direntry(struct superblock *sb, void **iter, struct dirent *dir);
static int sysfs_make_inode(struct superblock *sb, mode_t mode, void *info);

fs_ops sysfs_fsops = {
    .read_superblock = sysfs_read_superblock,
    .make_directory = sysfs_make_directory,
    .get_direntry = sysfs_get_direntry,
    .make_inode = sysfs_make_inode,
    .lookup_inode = sysfs_lookup_inode,
    .link_inode = sysfs_link_inode,
    .unlink_inode = sysfs_unlink_inode,
}; 

struct sysfs_data {
    struct dirent *rootdir,
};

fsdriver_t sysfs_driver = {
    .name = "sysfs",
    .fs_id = SYSFS_ID, /* "SYS" */
    .ops = &sysfs_fsops,
};

struct dirent sysfs_inode_table[] = {
};

static int sysfs_read_superblock(mountnode *sb) {
    sb->dev = gnu_dev_makedev(CHR_VIRT, CHR0_SYSFS);
    sb->data = { .rootdir = 
    return 0;
}

static int sysfs_make_directory(mountnode *sb, inode_t *ino, const char *path, mode_t mode) {
    return -ETODO;
}

static int sysfs_get_direntry(mountnode *sb, inode_t ino, void **iter, struct dirent *dir) {
}
#endif

/*
 *  ramfs
 */

#define RAMFS_ID  0x004d4152

static int ramfs_read_superblock(mountnode *sb);
static int ramfs_lookup_inode(mountnode *sb, inode_t *ino, const char *path);

struct fs_ops ramfs_fsops = {
    .read_superblock = ramfs_read_superblock,
    .lookup_inode = ramfs_lookup_inode,
};

/* this structure is a "hierarchical" lookup table from any large index to some pointer */
struct btree_node {
    int bt_level;         /* if 0, bt_children are leaves */
    size_t bt_fanout;     /* how many children for a node */
    void* bt_children[0]; /* child BTree nodes or leaves */
};

/* this is a directory hashtable array entry */
/* it is an intrusive list */
struct ramfs_direntry {
    uint32_t  name_hash;
    char     *entry_name;           /* its length should be a power of 2 */
    inode_t   ino;                  /* the actual value, inode_t */

    struct ramfs_direntry *htnext;  /* next in hashtable collision list */
};

/* this is a container for directory hashtable */
struct ramfs_directory {
    size_t size;                /* number of values in the hashtable */
    size_t htcap;               /* hashtable capacity */
    struct ramfs_direntry **ht;  /* hashtable array */
};

/* used by struct superblock as `data` pointer to store FS-specific state */
struct ramfs_data {
    struct btree_node *inodes_btree;  /* map from inode_t to struct inode */
    struct ramfs_directory *rootdir;   /* root directory entry */
};


struct fsdriver ramfs_driver = {
    .name = "ramfs",
    .fs_id = RAMFS_ID,
    .ops = &ramfs_fsops,
    .lst = { 0 },
};


static int ramfs_read_superblock(mountnode *sb) {
    const size_t fanout = 64;
    sb->blksz = PAGE_SIZE;

    struct ramfs_data *data = malloc(sizeof(struct ramfs_data));
    if (!data) goto enomem_exit;

    /* btree_node init */
    struct btree_node *inodes_btree = malloc(sizeof(struct btree_node) + sizeof(size_t) * fanout);
    if (!inodes_btree) goto enomem_exit;

    inodes_btree->bt_level = 0;
    memset(&inodes_btree->bt_children, 0, fanout * sizeof(size_t));
    /* end of btree_inode init */

    data->inodes_btree = inodes_btree;

    /* TODO: init rootdir */

    sb->data = (void *)data;
    sb->fs = &ramfs_driver;
    return 0;

enomem_exit:
    free(inodes_btree);
    free(data);
    return ENOMEM;
}


static inode * ramfs_idata_by_inode(mountnode *sb, inode_t ino) {
    struct ramfs_data *data = sb->data;
    int i;

    struct btree_node *btnode = data->inodes_btree;
    int btree_lvl = btnode->bt_level;
    size_t btree_blksz = btnode->bt_fanout;

    size_t last_ind = btree_blksz;
    for (i = 0; i < btree_lvl; ++i) {
        last_ind *= btree_blksz;
    }

    if (last_ind - 1 < ino)
        return NULL; // ENOENT

    /* get btree item */
    while (btree_lvl > 0) {
        size_t this_node_index = ino / last_ind;
        ino = ino % last_ind;
        struct btree_node *subnode = btnode->bt_children[ this_node_index ];
        if (!subnode) return NULL;
        btnode = subnode;
    }
    return btnode->bt_children[ ino ];
}

/*
 *    Searches given `dir` for part between `basename` and `basename_end`
 */
static int ramfs_get_inode_by_basename(struct ramfs_directory *dir, inode_t *ino, const char *basename, const char *basename_end) 
{
    size_t basename_len = basename_end - basename;
    uint32_t hash = strhash(basename, basename_len);

    /* directory hashtable: get direntry */
    struct ramfs_direntry *de = dir->ht[ hash % dir->htcap ];

    while (de) {
        if ((de->name_hash == hash) && !strncmp(basename, de->entry_name, basename_len)) {
            if (ino) *ino = de->ino;
            return 0;
        }
        de = de->htnext;
    }

    if (ino) *ino = 0;
    return ENOENT;
}


static int ramfs_lookup_inode(mountnode *sb, inode_t *result, const char *path)
{
    struct ramfs_data *data = sb->data;

    //     "some/longdirectoryname/to/examplefilename"
    //           ^                ^
    //       basename        basename_end
    const char *basename = path;
    const char *basename_end = path;
    inode_t ino;
    int ret = 0;

    struct ramfs_directory *dir = data->rootdir;
    for (;;) {
        for (;;) {
            switch (*++basename_end) {
                case FS_SEP :
                    /* to to directory search */
                    break;
                case 0:
                    /* basename is "examplefilename" now */
                    ret = ramfs_get_inode_by_basename(dir, &ino, basename, basename_end);
                    if (result) *result = (ret ? 0 : ino);
                    return ret;
            }
        }

        ret = ramfs_get_inode_by_basename(dir, &ino, basename, basename_end);
        if (!ret) {
            if (result) *result = (ret ? 0 : ino);
            return ret;
        }
        inode *idata = ramfs_idata_by_inode(sb, ino);
        if (! S_ISDIR(idata->mode)) {
            if (result) *result = 0;
            return ENOTDIR;
        }

        while (*++basename_end == FS_SEP);
        basename = basename_end;
    }
}


/*
 *  VFS operations
 */

err_t vfs_mount(dev_t source, const char *target, const mount_opts_t *opts) {
    if (theRootMnt == NULL) {
        if ((target[0] != '/') || (target[1] != '\0')) {
             logmsgef("vfs_mount: mount('%s') with no root", target);
             return -1;
        }

        fsdriver *fs = vfs_fs_by_id(opts->fs_id);
        if (!fs) return -2;

        struct superblock *sb = kmalloc(sizeof(struct superblock));
        return_err_if(!sb, -3, "vfs_mount: malloc(superblock) failed");

        sb->fs = fs;
        int ret = fs->ops->read_superblock(sb);
        return_err_if(ret, -4, "vfs_mount: read_superblock failed (%d)", ret);

        sb->parent = NULL;
        sb->brother = NULL;
        sb->childs = NULL;
        sb->relmntpath = NULL;

        theRootMnt = sb;
        return 0;
    }
    logmsge("vfs_mount: TODO: non-root");
    return -110;
}

err_t vfs_mkdir(const char *path, mode_t mode) {
    return -ETODO;
}



void print_ls(const char *path) {
    logmsgef("TODO: ls not implemented\n");
}



void print_mount(void) {
    struct superblock *sb = theRootMnt;
    if (!sb) return;

    k_printf("%s on /\n", sb->fs->name);
    if (sb->childs) {
        logmsge("TODO: print child mounts");
    }
}

void vfs_setup(void) {
    int ret;

    /* register filesystems here */
    vfs_register_filesystem(&ramfs_driver);

    /* mount actual filesystems */
    dev_t fsdev = gnu_dev_makedev(CHR_VIRT, CHR0_UNSPECIFIED);
    mount_opts_t mntopts = { .fs_id = RAMFS_ID };
    ret = vfs_mount(fsdev, "/", &mntopts);
    returnv_err_if(ret, "root mount on sysfs failed (%d)", ret);

    k_printf("%s on / mounted successfully\n", theRootMnt->fs->name);
}
