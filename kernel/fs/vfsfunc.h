// #include "vfs.h"
#pragma once

#include "types.h"
struct inode* namei(char *path);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iunlockput(struct inode *ip);
struct file* filealloc(void);
void fileclose(struct file *f);
struct inode* iget(uint dev, uint inum);
void iput(struct inode *ip);
#define min(a, b) ((a) < (b) ? (a) : (b))
int dirlink(struct inode *dp, char *name, uint inum);
struct file* filedup(struct file *f);
int fileread(struct file *f, uint64 addr, int n);
int filewrite(struct file *f, uint64 addr, int n);
int filestat(struct file *f, uint64 addr);
struct inode* nameiparent(char *path, char *name);
int namecmp(const char *s, const char *t);
void iinit();
void fileinit(void);
struct inode* idup(struct inode *ip);
void fsinit(int dev);


#define ROOTINO  1   // root i-number
#define CONSOLE 1