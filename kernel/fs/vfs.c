
#include "vfs.h"
#include "defs.h"
#include "xv6fs/xv6_vfs.h"
// #include <stdio.h>



// static struct inode* iget(uint dev, uint inum);
struct {
  struct inode inode[NINODE];
} itable;



struct inode*
iget(uint dev, uint inum)
{
  // printf("enter iget\n");

  struct inode *ip, *empty;

  empty = 0;
  for(ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      // printf("exit iget\n");
      return ip;
    }
    if(empty == 0 && ip->ref == 0)
      empty = ip;
  }

  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->op = &xv6fs_ops;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->private = 0;

  // printf("exit iget\n");
  return ip;
}

struct super_block *root;

void
fsinit(int dev) {
  //printf("enter fsinit\n");
  
  root = kalloc();
  
  root->type = &xv6fs;
  root->op = &xv6fs_ops;
  root->op->init();
  root->root = iget(1,1);
  root->parent = 0;
  root->mountpoint = 0;
  // root->private = kalloc();
  root->private = &sb;

  //printf("exit fsinit\n");
}

void
iinit()
{
  //printf("enter iinit\n");

  int i = 0;
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&itable.inode[i].lock, "inode");
  }

  //printf("exit iinit\n");
}

static char*
skipelem(char *path, char *name)
{
  //printf("enter skipelem\n");

  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0) {
    //printf("exit skipelem\n");
    return 0;
  }
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  
  //printf("exit skipelem\n");
  return path;
}

int
namecmp(const char *s, const char *t)
{
  // printf("enter namecmp\n");
  // if(t==0)printf("ggggggggggg\n");
  int ret = strncmp(s, t, DIRSIZ);

  // printf("exit namecmp\n");
  return ret;
}

void
iunlock(struct inode *ip)
{
  // printf("enter iunlock\n");

  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);

  // printf("exit iunlock\n");
}

void
iput(struct inode *ip)
{
  // printf("enter iput\n");

  if(ip->ref == 1 && ip->private != 0 && ip->nlink == 0){
    acquiresleep(&ip->lock);
    ip->op->trunc(ip);
    ip->type = 0;
    ip->op->write_inode(ip);
    kfree(ip->private);
    ip->private = 0;
    releasesleep(&ip->lock);
  }

  ip->ref--;

  // printf("exit iput\n");
}

void
iunlockput(struct inode *ip)
{
  // printf("enter iunlockput\n");

  iunlock(ip);
  iput(ip);

  // printf("exit iunlockput\n");
}

struct inode*
idup(struct inode *ip)
{
  // printf("enter idup\n");

  ip->ref++;
  
  // printf("exit idup\n");
  return ip;
}

void
ilock(struct inode *ip)
{
  // printf("enter ilock\n");

  if(ip == 0 || ip->ref < 1)
    panic("ilock");
  
  acquiresleep(&ip->lock);
  
  if(ip->private == 0){
    if(ip->op==0)printf("ffffffffffffffff\n");
    ip->op->update_inode(ip);
  }
  // printf("----inum:%d\n", ip->inum);

  // printf("exit ilock\n");
}

static struct inode*
namex(char *path, int nameiparent, char *name)
{
  // printf("enter namex\n");

  struct inode *ip, *next;
  struct dentry *de;
  if(*path == '/'){
    if(root == 0){
      ip = iget(ROOTDEV, ROOTINO);
    }else ip = root->root;
  }else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      // printf("exit namex\n");
      return 0;
    }
    if(nameiparent && *path == '\0'){
      iunlock(ip);
      // printf("exit namex\n");
      return ip;
    }
    de = ip->op->dirlookup(ip,name);
    if((next = de->inode) == 0){
      iunlockput(ip);
      kfree(de->private);
      kfree(de);
      // printf("exit namex\n");
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    // printf("exit namex\n");
    return 0;
  }
  // printf("exit namex\n");
  return ip;
}

struct inode*
namei(char *path)
{
  //printf("enter namei\n");

  char name[DIRSIZ];
  struct inode *ret = namex(path, 0, name);

  //printf("exit namei\n");
  return ret;
}

struct inode*
nameiparent(char *path, char *name)
{
  //printf("enter nameiparent\n");

  struct inode *ret = namex(path, 1, name);

  //printf("exit nameiparent\n");
  return ret;
}

void
stati(struct inode *ip, struct stat *st)
{
  //printf("enter stati\n");

  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;

  //printf("exit stati\n");
}

int
dirlink(struct inode *dp, char *name, uint inum)
{
  // printf("enter dirlink\n");
  
  struct dentry *d;
  d = kalloc();
  // d->inode = kalloc();
  d->private = kalloc();
  d->parent = dp;
  // printf("shit\n");
  *(uint *)(d->private) = inum;
  // printf("shit\n");
  // d->inode->inum = inum;
  
  strncpy(d->name, name, DIRSIZ);
  
  int ret = dp->op->link(d);

  // kfree(d->inode);
  kfree(d->private);
  kfree(d);

  // printf("exit dirlink\n");
  return ret;
}


struct devsw devsw[NDEV];
struct {
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  //printf("enter fileinit\n");

  // No implementation provided, assuming initialization code here

  //printf("exit fileinit\n");
}

struct file*
filealloc(void)
{
  //printf("enter filealloc\n");

  struct file *f;
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;

      //printf("exit filealloc\n");
      return f;
    }
  }

  //printf("exit filealloc\n");
  return 0;
}

struct file*
filedup(struct file *f)
{
  //printf("enter filedup\n");

  if(f->ref < 1)
    panic("filedup");
  f->ref++;

  //printf("exit filedup\n");
  return f;
}

void
fileclose(struct file *f)
{
  //printf("enter fileclose\n");

  f->op->close(f);

  //printf("exit fileclose\n");
}

int
filestat(struct file *f, uint64 addr)
{
  // printf("enter filestat\n");

  struct proc *p = myproc();
  struct stat st;

  if(f->type == FD_INODE || f->type == FD_DEVICE){
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0) {
      // printf("exit filestat\n");
      return -1;
    }
    // printf("exit filestat\n");
    return 0;
  }
  // printf("exit filestat\n");
  return -1;
}

int
fileread(struct file *f, uint64 addr, int n)
{
  // printf("enter fileread\n");

  int r = 0;

  if(f->readable == 0) {
    // printf("exit fileread\n");
    return -1;
  }

  if(f->type == FD_DEVICE){
    // if( !devsw[CONSOLE].read) {
    //   printf("exit fileread\n");
    //   return -1;
    // }
    r = devsw[CONSOLE].read(1, addr, n);
  } else if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = f->op->read(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  }

  // printf("exit fileread\n");
  return r;
}

int
filewrite(struct file *f, uint64 addr, int n)
{
  // printf("enter filewrite\n");

  int r, ret = 0;

  if(f->writable == 0) {
    // printf("exit filewrite\n");
    return -1;
  }

  if(f->type == FD_DEVICE){
    // if(!devsw[CONSOLE].write) {
    //   printf("exit filewrite\n");
    //   return -1;
    // }
    ret = devsw[CONSOLE].write(1, addr, n);
  } else if(f->type == FD_INODE){
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      ilock(f->ip);
      if ((r = f->op->write(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);

      if(r != n1)
        break;
      i += r;
    }
    ret = (i == n ? n : -1);
  }

  // printf("exit filewrite\n");
  return ret;
}
