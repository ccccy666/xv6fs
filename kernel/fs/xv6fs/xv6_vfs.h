#pragma once

#include "kernel/defs.h"
#include "defs.h"
#include "buf.h"
#include "file.h"

#include "kernel/fs/vfs.h"
#include "proc.h"
// struct super_block *xv6_mount (const char *source);
  // Unmount a filesystem.
  // Ignored at this stage.
  // Linux: super_operations->umount_begin
// int xv6_umount (struct super_block *sb);
// Allocate an inode in the inode table on disk.
// Linux: super_operations->alloc_inode
struct inode *xv6_alloc_inode (struct super_block *ssb,short type);
// Write (update) an existing inode.
// Linux: super_operations->write_inode
void xv6_write_inode (struct inode *ino);
// Called when the inode is recycled.
// Linux: super_operations->evict_inode
// void xv6_release_inode (struct inode *ino);
// Free the inode in the inode table on disk.
// Linux: super_operations->free_inode
// void xv6_free_inode (struct inode *ino);
// Truncate the file corresponding to inode.
// Linux: (none)
void xv6_trunc (struct inode *ino);
// Opens (returns a file instance) of the inode.
// Linux: inode_operations->atomic_open
struct file *xv6_open (struct inode *ino, uint mode);
// Closes an open file.
// Linux: file_operations->flush
void xv6_close (struct file *f);
// Reads from the file.
// If dst_is_user==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
// Linux: file_operations->read
int xv6_read (struct inode *ino, char dst_is_user, uint64 dst, uint off, uint n);
// Writes to the file.
// Linux: file_operations->write
int xv6_write (struct inode *ino, char src_is_user, uint64 src, uint off, uint n);
// Creates a new file.
// target is a newly created dentry; target->inode is the actual file.
// Linux: inode_operations->create
int xv6_create (struct inode *dir, struct dentry *target, short type, short major, short minor);
// Creates a new link.
// target is a newly created dentry; target->inode is the actual file.
// Linux: inode_operations->link
int xv6_link (struct dentry *target);
// Removes a link, and deletes a file if it is the last link.
// Linux: inode_operations->unlink
int xv6_unlink (struct dentry *d);
// look for a file in the directory.
// Linux: inode_operations->lookup
struct dentry *xv6_dirlookup (struct inode *dir, const char *name);
// Called when the dentry is recycled.
// Linux: dentry_operations->d_release
// void xv6_release_dentry (struct dentry *de);
// Is the directory dp empty except for "." and ".." ?
// Linux: (none)
int xv6_isdirempty (struct inode *dir);
// initialize filesystem type
// Linux: (none)
void xv6_init (void);

void xv6_update_inode(struct inode *ip);

extern struct filesystem_operations xv6fs_ops;



extern struct filesystem_type xv6fs;