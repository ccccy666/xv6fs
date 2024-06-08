// #include "kernel/fs/vfs.h"

#include "defs.h"
#include "fs/vfs.h"
#include "fs/xv6fs/fs.h"
#include "param.h"
#include "types.h"
#include "xv6_vfs.h"
#include "file.h"
#include "xv6_fcntl.h"
// #include <stdio.h>
// #include "kernel/fs/sysfile.c"
struct xv6fs_super_block sb;
// Read the super block.
static void readsb(int dev, struct xv6fs_super_block *sb) {
    // printf("enter readsb\n");

    struct buf *bp;
    bp = bread(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    brelse(bp);

    // printf("exit readsb\n");
}

// Zero a block.
static void bzero(int dev, int bno) {
    // printf("enter bzero\n");

    struct buf *bp;
    bp = bread(dev, bno);
    memset(bp->data, 0, BSIZE);
    bwrite(bp);
    brelse(bp);

    // printf("exit bzero\n");
}

// Allocate a zeroed disk block. Returns 0 if out of disk space.
static uint balloc(uint dev) {
    // printf("enter balloc\n");

    int b, bi, m;
    struct buf *bp;

    bp = 0;
    for(b = 0; b < sb.size; b += BPB){
        bp = bread(dev, BBLOCK(b, sb));
        for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
            m = 1 << (bi % 8);
            if((bp->data[bi/8] & m) == 0){  // Is block free?
                bp->data[bi/8] |= m;  // Mark block in use.
                bwrite(bp);
                brelse(bp);
                bzero(dev, b + bi);
                // printf("exit balloc\n");
                return b + bi;
            }
        }
        brelse(bp);
    }
    printf("balloc: out of blocks\n");
    // printf("exit balloc\n");
    return 0;
}

// Free a disk block.
static void bfree(int dev, uint b) {
    // printf("enter bfree\n");

    struct buf *bp;
    int bi, m;

    bp = bread(dev, BBLOCK(b, sb));
    bi = b % BPB;
    m = 1 << (bi % 8);
    if((bp->data[bi/8] & m) == 0)
        panic("freeing free block");
    bp->data[bi/8] &= ~m;
    bwrite(bp);
    brelse(bp);

    // printf("exit bfree\n");
}

// Return the disk block address of the nth block in inode ip. If there is no such block, bmap allocates one. Returns 0 if out of disk space.
static uint bmap(struct xv6fs_inode *ip, uint bn) {
    // printf("enter bmap\n");

    uint addr, *a;
    struct buf *bp;

    if(bn < NDIRECT){
        if((addr = ip->addrs[bn]) == 0){
            addr = balloc(ip->dev);
            if(addr == 0) {
                // printf("exit bmap\n");
                return 0;
            }
            ip->addrs[bn] = addr;
        }
        // printf("exit bmap\n");
        return addr;
    }
    bn -= NDIRECT;

    if(bn < NINDIRECT){
        if((addr = ip->addrs[NDIRECT]) == 0){
            addr = balloc(ip->dev);
            if(addr == 0) {
                // printf("exit bmap\n");
                return 0;
            }
            ip->addrs[NDIRECT] = addr;
        }
        bp = bread(ip->dev, addr);
        a = (uint*)bp->data;
        if((addr = a[bn]) == 0){
            addr = balloc(ip->dev);
            if(addr){
                a[bn] = addr;
                bwrite(bp);
            }
        }
        brelse(bp);
        // printf("exit bmap\n");
        return addr;
    }

    panic("bmap: out of range");
}

struct inode *xv6_alloc_inode(struct super_block *ssb,short type) {
    // printf("enter xv6_alloc_inode\n");

    // struct xv6fs_super_block *sbb = sb->private;
    int inum;
    struct buf *bp;
    struct dinode *dip;

    for(inum = 1; inum < sb.ninodes; inum++){
        if(ssb->root->dev != 1)panic("cyfffff");
        bp = bread(ssb->root->dev, IBLOCK(inum, sb));
        dip = (struct dinode*)bp->data + inum%IPB;
        if(dip->type == 0){  // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            bwrite(bp);   // mark it allocated on the disk
            brelse(bp);
            struct inode *in = iget(ssb->root->dev, inum);
            in->op = &xv6fs_ops;
            
            in->sb = root;
            in->private = 0;
            // printf("exit xv6_alloc_inode\n");
            return in;
        }
        brelse(bp);
    }
    printf("ialloc: no inodes\n");
    // printf("exit xv6_alloc_inode\n");
    return 0;
}

void xv6_write_inode(struct inode *ino) {
    // printf("enter xv6_write_inode\n");

    struct xv6fs_inode *ip = ino->private;
    struct buf *bp;
    struct dinode *dip;

    bp = bread(ino->dev, IBLOCK(ino->inum, sb));
    dip = (struct dinode*)bp->data + ino->inum%IPB;
    dip->type = ino->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ino->nlink;
    dip->size = ino->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    bwrite(bp);
    brelse(bp);

    // printf("exit xv6_write_inode\n");
}

void xv6_free_inode(struct inode *ino) {
    //printf("enter xv6_free_inode\n");

    // Implementation for freeing an inode (not provided in the original code)

    //printf("exit xv6_free_inode\n");
}

void xv6_trunc(struct inode *ino) {
    // printf("enter xv6_trunc\n");

    struct xv6fs_inode *ip = ino->private;
    int i, j;
    struct buf *bp;
    uint *a;

    for(i = 0; i < NDIRECT; i++){
        if(ip->addrs[i]){
            if(ip->dev != 1)panic("cyfffff");
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if(ip->addrs[NDIRECT]){
        bp = bread(ip->dev, ip->addrs[NDIRECT]);
        a = (uint*)bp->data;
        for(j = 0; j < NINDIRECT; j++){
            if(a[j])
                bfree(ip->dev, a[j]);
        }
        brelse(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    ino->size = 0;
    xv6_write_inode(ino);

    // printf("exit xv6_trunc\n");
}

struct file *xv6_open(struct inode *ino, uint mode) {
    // printf("enter xv6_open\n");

    struct file *f;
    struct xv6fs_inode *ip = ino->private;

    if(ino->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
        iunlockput(ino);
        return 0;
    }

    if((f = filealloc()) == 0 ){
        iunlockput(ino);
        // printf("exit xv6_open\n");
        return 0;
    }

    // struct xv6fs_file *ff = f->private;
    if(ino->type == T_DEVICE){
        f->type = FD_DEVICE;
        // ff->major = ip->major;
    } else {
        f->type = FD_INODE;
        f->off = 0;
    }
    f->ip = ino;
    f->readable = !(mode & O_WRONLY);
    f->writable = (mode & O_WRONLY) || (mode & O_RDWR);
    f->op = ino->op;
    if((mode & O_TRUNC) && ino->type == T_FILE){
        ino->op->trunc(ino);
    }

    // printf("exit xv6_open\n");
    return f;
}

void xv6_close(struct file *f) {
    // printf("enter xv6_close\n");

    struct file ff ;

    if(f->ref < 1)
        panic("fileclose");
    if(--f->ref > 0){
        // printf("exit xv6_close\n");
        return;
    }

    ff = *f;
    // struct xv6fs_file *ff_ = ff->private;
    f->ref = 0;
    f->type = FD_NONE;

    if(ff.type == FD_PIPE){
        // pipeclose(ff_->pipe, ff->writable);
    } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
        iput(ff.ip);
    }
    // kfree(ff);

    // printf("exit xv6_close\n");
}

int xv6_read(struct inode *ino, char dst_is_user, uint64 dst, uint off, uint n) {
    // printf("enter xv6_read\n");

    uint tot, m;
    struct buf *bp;
    struct xv6fs_inode *ip = ino->private;

    if(off > ino->size || off + n < off) {
        // printf("exit xv6_read\n");
        return 0;
    }
    if(off + n > ino->size)
        n = ino->size - off;

    for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
        uint addr = bmap(ip, off/BSIZE);
        if(addr == 0)
            break;
        bp = bread(ip->dev, addr);
        m = min(n - tot, BSIZE - off%BSIZE);
        if(either_copyout(dst_is_user, dst, bp->data + (off % BSIZE), m) == -1) {
            brelse(bp);
            tot = -1;
            break;
        }
        brelse(bp);
    }

    // printf("exit xv6_read\n");
    return tot;
}

int xv6_write(struct inode *ino, char src_is_user, uint64 src, uint off, uint n) {
    // printf("enter xv6_write\n");

    struct xv6fs_inode *ip = ino->private;
    uint tot, m;
    struct buf *bp;
    // printf("111111111111\n");
    if(off > ino->size || off + n < off) {
        // printf("exit xv6_write\n");
        return -1;
    }
    // printf("222222222\n");
    if(off + n > MAXFILE*BSIZE) {
        // printf("exit xv6_write\n");
        return -1;
    }
    // printf("33333333333\n");
    for(tot=0; tot<n; tot+=m, off+=m, src+=m){
        uint addr = bmap(ip, off/BSIZE);
        if(addr == 0)
            break;
        bp = bread(ip->dev, addr);
        m = min(n - tot, BSIZE - off%BSIZE);
        // printf()
        // if(bp->data == 0 && src_is_user ==0)printf("6666666666\n");
        if(either_copyin(bp->data + (off % BSIZE), src_is_user, src, m) == -1) {
            // printf("%d\n",tot);
            brelse(bp);
            break;
        }
        // printf("%d\n",tot);
        bwrite(bp);
        brelse(bp);
    }
    // printf("4444444444444\n");
    if(off > ino->size)
        ino->size = off;

    xv6_write_inode(ino);

    // printf("exit xv6_write\n");
    return tot;
}

int xv6_create(struct inode *dir, struct dentry *target, short type, short major, short minor) {
    // printf("enter xv6_create\n");

    struct inode *ip;

    ilock(dir);
    struct dentry *dd = dir->op->dirlookup(dir, target->name);
    if((ip = dd->inode) != 0){
        iunlockput(dir);
        ilock(ip);
        if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE)){
            target->inode = ip;
            kfree(dd->private);
            kfree(dd);
            // printf("exit xv6_create\n");
            return 0;
        }
        iunlockput(ip);
        kfree(dd->private);
        kfree(dd);
        // printf("exit xv6_create\n");
        return 1;
    }

    if((ip = xv6_alloc_inode(root,type)) == 0){
        
        iunlockput(dir);
        kfree(dd->private);
        kfree(dd);
        // printf("exit xv6_create\n");
        return 1;
    }
    ilock(ip);
    
    struct xv6fs_inode *in = ip->private;
    struct xv6fs_dentry *tar = target->private;
    strncpy(tar->name, target->name , DIRSIZ);
    
    in->major = major;
    in->minor = minor;
    ip->nlink = 1;
    ip->op->write_inode(ip);

    if(type == T_DIR){  // Create . and .. entries.
        if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dir->inum) < 0)
            goto fail;
    }
    // printf("fuck\n");
    if(dirlink(dir, target->name, ip->inum) < 0)
        goto fail;

    if(type == T_DIR){
        dir->nlink++;  // for ".."
        dir->op->write_inode(dir);
    }

    iunlockput(dir);
    target->inode = ip;
    kfree(dd->private);
    kfree(dd);
    // printf("exit xv6_create\n");
    return 0;

fail:
    ip->nlink = 0;
    ip->op->write_inode(ip);
    iunlockput(ip);
    iunlockput(dir);
    kfree(dd->private);
    kfree(dd);
    // printf("exit xv6_create\n");
    return 1;
}

int xv6_link(struct dentry *target) {
    // printf("enter xv6_link\n");

    int off;
    uint inum = *(uint *)(target->private);
    struct inode *dp = target->parent;
    struct xv6fs_dentry de;
    struct inode *ip;
    struct dentry *dir = xv6_dirlookup(dp, target->name);
    
    if((ip = dir->inode) != 0){
        iput(ip);
        kfree(dir->private);
        kfree(dir);
        // printf("exit xv6_link\n");
        return -1;
    }

    for(off = 0; off < dp->size; off += sizeof(de)){
        if(xv6_read(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)){
            kfree(dir->private);
            kfree(dir);
            panic("dirlink read");
        }
            
        if(de.inum == 0)
            break;
    }
    strncpy(de.name, target->name, DIRSIZ);
    de.inum = inum;
    if(xv6_write(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) {
        kfree(dir->private);
        kfree(dir);
        // printf("exit xv6_link\n");
        return -1;
    }

    kfree(dir->private);
    kfree(dir);
    // printf("exit xv6_link\n");
    return 0;
}

struct dentry *xv6_dirlookup(struct inode *dir, const char *name) {
    // printf("enter xv6_dirlookup\n");
    // printf("create name:%s\n",name);
    uint off, inum;
    struct dentry *de = kalloc();
    de->private = kalloc();
    struct xv6fs_dentry dee;

    if(dir->type != T_DIR)
        panic("dirlookup not DIR");

    for(off = 0; off < dir->size; off += sizeof(dee)){
        if(xv6_read(dir, 0, (uint64)&dee, off, sizeof(dee)) != sizeof(dee))
            panic("dirlookup read");
        if(dee.inum == 0)
            continue;
        if(de == 0)printf("gggggggggg\n");
        // printf("%s\n",dee.name);
        if(namecmp(name, dee.name) == 0){
            inum = dee.inum;
            de->inode = iget(dir->dev, inum);
            de->op = &xv6fs_ops;
            *(uint*)(de->private) = off;
            // printf("exit xv6_dirlookup\n");
            return de;
        }
    }

    kfree(de);
    // printf("exit xv6_dirlookup\n");
    struct dentry *ret = kalloc();
    ret->private = kalloc();
    ret->inode = 0;
    return ret;
}

int xv6_unlink (struct dentry *d){
//   printf("enter unlink\n");  
  struct inode *ip, *dp;
  struct xv6fs_dentry de;
//   de = kalloc();
  char name[DIRSIZ], path[MAXPATH];
  strncpy(path, (char *)(d->private), MAXPATH);

  uint off = 0;


  if((dp = nameiparent(path, name)) == 0){
    return -1;
  }
  // strncpy(de.name, name, DIRSIZ);
  // de.inode=dp;
  // dp->op->unlink(&de);
  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0){
    iunlockput(dp);
    return -1;
  }
  struct dentry *dd = dp->op->dirlookup(dp, name);
//   if(dd->inode != 0) goto bad;
  off = *(uint*)(dd->private);
//   // dd 

//   // struct buf *bp;
//   // struct dinode *dip;
//   // struct xv6fs_inode *dp= dir->private;
//   uint poff;
//   struct dentry *dde = kalloc();
// //   dde->private = kalloc();
// //   struct xv6fs_dentry *ddd = dde->private;
//   struct xv6fs_dentry dee;
//   if(dp->type != T_DIR)
//     panic("dirlookup not DIR");

//   for(poff = 0; poff < dp->size;poff += sizeof(dee)){
//     if(dp->op->read(dp, 0, (uint64)&dee, poff, sizeof(dee)) != sizeof(dee))
//       panic("dirlookup read");
//     if(dee.inum == 0)
//       continue;
//     if(namecmp(name, dee.name) == 0){
//       // entry matches path element
      
//       off = poff;
//       break;
//       // inum = dee->inum;
//       // dde->inode = iget(dp->dev, inum);
//       //struct dentry *d = &de;
//       // return de;
//       //return iget(dir->dev, inum);
//     }
//   }
// //   kfree(dde->private);
//   kfree(dde);
  
  if((ip = dd->inode) == 0)
    goto bad;
  //off = dd.inode.
  
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !xv6_isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));

  if(dp->op->write(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    dp->op->write_inode(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  dp->op->write_inode(ip);
  iunlockput(ip);
  kfree(dd->private);
  kfree(dd);
//   kfree(de);
//   printf("exit unlink\n");  
  return 0;

bad:
  kfree(dd->private);
  kfree(dd);
  iunlockput(dp);
//   printf("exit unlink\n");  
  return -1;
};

// void xv6_release_dentry(struct dentry *de) {
//     printf("enter xv6_release_dentry\n");

//     // Implementation for releasing a dentry (not provided in the original code)

//     printf("exit xv6_release_dentry\n");
// }

int xv6_isdirempty(struct inode *dir) {
    //printf("enter xv6_isdirempty\n");

    int off;
    
    struct xv6fs_dentry d ;

    for(off=2*sizeof(d); off<dir->size; off+=sizeof(d)){
        if(xv6_read(dir, 0, (uint64)&d, off, sizeof(d)) != sizeof(d))
            panic("isdirempty: readi");
        if(d.inum != 0){
            
            // printf("exit xv6_isdirempty\n");
            return 0;
        }
    }

    
    //printf("exit xv6_isdirempty\n");
    return 1;
}

void xv6_init(void) {
    //printf("enter xv6_init\n");

    readsb(1, &sb);
    if(sb.magic != FSMAGIC)
        panic("invalid file system");

    //printf("exit xv6_init\n");
}

void xv6_update_inode(struct inode *ip) {
    // printf("enter xv6_update_inode\n");

    struct buf *bp;
    struct dinode *dip;
    ip->private = kalloc();
    struct xv6fs_inode *in = ip->private;
    
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    // printf("gggggggggggg\n");
    ip->type = dip->type;
    // printf("***%d***\n", dip->type);
    in->dev = 1;
    in->major = dip->major;
    in->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    
    memmove(in->addrs, dip->addrs, sizeof(in->addrs));
    brelse(bp);

    if(ip->type == 0)
        panic("ilock: no type");

    // printf("exit xv6_update_inode\n");
}

struct filesystem_operations xv6fs_ops = {
    .update_inode = xv6_update_inode,
    .alloc_inode = xv6_alloc_inode,
    .write_inode = xv6_write_inode,
    .free_inode = xv6_free_inode,
    .trunc = xv6_trunc,
    .open = xv6_open,
    .close = xv6_close,
    .read = xv6_read,
    .write = xv6_write,
    .create = xv6_create,
    .link = xv6_link,
    .dirlookup = xv6_dirlookup,
    .isdirempty = xv6_isdirempty,
    .init = xv6_init,
    .unlink = xv6_unlink,
};


struct filesystem_type xv6fs = {
    .type = "xv6fs",
    .op = &xv6fs_ops,
};
