#include "ext2.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

/* The FUSE version has to be defined before any call to relevant
   includes related to FUSE. */
#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif
#include <fuse.h>

static void *ext2_init(struct fuse_conn_info *conn);
static void ext2_destroy(void *private_data);
static int ext2_getattr(const char *path, struct stat *stbuf);
static int ext2_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi);
static int ext2_open(const char *path, struct fuse_file_info *fi);
static int ext2_release(const char *path, struct fuse_file_info *fi);
static int ext2_read(const char *path, char *buf, size_t size, off_t offset,
		     struct fuse_file_info *fi);
static int ext2_readlink(const char *path, char *buf, size_t size);

static volume_t *volume;

static const struct fuse_operations ext2_operations = {
  .init = ext2_init,
  .destroy = ext2_destroy,
  .open = ext2_open,
  .read = ext2_read,
  .release = ext2_release,
  .getattr = ext2_getattr,
  .readdir = ext2_readdir,
  .readlink = ext2_readlink,
};

 struct file_handler {
  inode_t inode;
  int processor;
  int lock;
};

int main(int argc, char *argv[]) {
  
  char *volumefile = argv[--argc];
  volume = open_volume_file(volumefile);
  argv[argc] = NULL;
  
  if (!volume) {
    fprintf(stderr, "Invalid volume file: '%s'.\n", volumefile);
    exit(1);
  }
  
  fuse_main(argc, argv, &ext2_operations, volume);
  
  return 0;
}

/* ext2_init: Function called when the FUSE file system is mounted.
 */
static void *ext2_init(struct fuse_conn_info *conn) {
  
  printf("init()\n");
  
  return NULL;
}

/* ext2_destroy: Function called before the FUSE file system is
   unmounted.
 */
static void ext2_destroy(void *private_data) {
  
  printf("destroy()\n");
  
  close_volume_file(volume);
}

/* ext2_getattr: Function called when a process requests the metadata
   of a file. Metadata includes the file type, size, and
   creation/modification dates, among others (check man 2
   fstat). Typically FUSE will call this function before most other
   operations, for the file and all the components of the path.
   
   Parameters:
     path: Path of the file whose metadata is requested.
     stbuf: Pointer to a struct stat where metadata must be stored.
   Returns:
     In case of success, returns 0, and fills the data in stbuf with
     information about the file. If the file does not exist, should
     return -ENOENT.
 */
static int ext2_getattr(const char *path, struct stat *stbuf) {

  /* TO BE COMPLETED BY THE STUDENT */
  //find the file and corresponding Inode
  uint32_t inode_no;
  inode_t inode;
  if (!(inode_no = find_file_from_path(volume, path, &inode))) {
    return -ENOENT;
  }else{
    stbuf->st_ino = inode_no;
    stbuf->st_mode = inode.i_mode;
    stbuf->st_nlink = inode.i_links_count;
    stbuf->st_uid = inode.i_uid;
    stbuf->st_gid = inode.i_gid;
    stbuf->st_size = inode_file_size(volume,&inode);
    stbuf->st_blksize = volume->block_size;
    stbuf->st_blocks = inode.i_blocks;
    stbuf->st_atime = inode.i_atime;
    stbuf->st_mtime = inode.i_mtime;
    stbuf->st_ctime = inode.i_ctime;
  }
  return 0; // Function not implemented
}

/* ext2_readdir: Function called when a process requests the listing
   of a directory.
   
   Parameters:
     path: Path of the directory whose listing is requested.
     buf: Pointer that must be passed as first parameter to filler
          function.
     filler: Pointer to a function that must be called for every entry
             in the directory.  Will accept four parameters, in this
             order: buf (previous parameter), the filename for the
             entry as a string, a pointer to a struct stat containing
             the metadata of the file (optional, may be passed NULL),
             and an offset for the next call to ext2_readdir
             (optional, may be passed 0).
     offset: Will usually be 0. If a previous call to filler for the
             same path passed a non-zero value as the offset, this
             function will be called again with the provided value as
             the offset parameter. Optional.
     fi: Not used in this implementation of readdir.

   Returns:
     In case of success, returns 0, and calls the filler function for
     each entry in the provided directory. If the directory doesn't
     exist, returns -ENOENT.
 */
static int ext2_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {

  /* TO BE COMPLETED BY THE STUDENT */
  //Find the directory file and corresponding Indoe
  inode_t inode;
  if (!find_file_from_path(volume, path, &inode)) {
    return -ENOENT; //Cannot find the file
  } else{
    if((inode.i_mode & S_IFMT) != S_IFDIR){
      return -ENOENT; //the file is not a directory
    }
    dir_entry_t entry;
    uint64_t fileSize = inode_file_size(volume,&inode);
    while (offset < fileSize) {
      read_file_content(volume, &inode, (uint32_t)offset, sizeof(dir_entry_t),&entry);
      if(entry.de_inode_no != 0) {
        struct stat filestat;
        ext2_getattr(path,&filestat);
        char temp[256];
        strcpy(temp,entry.de_name);
        temp[entry.de_name_len] = '\0';//if the Inode is 0/NULL,skip it.
        (filler)(buf,temp,&filestat,0);
      }
      offset += entry.de_rec_len;
    }
  }

  return 0;
}

/* ext2_open: Function called when a process opens a file in the file
   system.
   
   Parameters:
     path: Path of the file being opened.
     fi: Data structure containing information about the file being
         opened. Some useful fields include:
	 flags: Flags for opening the file. Check 'man 2 open' for
                information about which flags are available.
	 fh: File handle. The value you set to this handle will be
             passed unmodified to all other file operations involving
             the same file.
   Returns:
     In case of success, returns 0. In case of error, may return one
     of these error codes:
       -ENOENT: If the file does not exist;
       -EISDIR: If the path corresponds to a directory;
       -EACCES: If the open operation was for writing, and the file is
                read-only.
 */
static int ext2_open(const char *path, struct fuse_file_info *fi) {
  
  if (fi->flags & O_WRONLY || fi->flags & O_RDWR)
    return -EACCES;

  /* TO BE COMPLETED BY THE STUDENT */
  inode_t inode;
  if (!find_file_from_path(volume, path, &inode)) {
    return -ENOENT; //Cannot find the file
  }else {
    if ((inode.i_mode & S_IFMT) == S_IFDIR) {
      return -EISDIR; //the file is a directory
    }
    if(fi->fh == 0){
      struct file_handler *fh = malloc(sizeof(struct file_handler));
      fi->fh = (uint64_t)fh;
      fh->processor += 1;
      fh->inode = inode;
    } else{
      struct file_handler *fh = (struct file_handler *) fi->fh;
      fh->processor += 1;
    }
  }
  return 0; // Function not implemented
}

/* ext2_release: Function called when a process closes a file in the
   file system. If the open file is shared between processes, this
   function is called when the file has been closed by all processes
   that share it.
   
   Parameters:
     path: Path of the file being closed.
     fi: Data structure containing information about the file being
         opened. This is the same structure used in ext2_open.
   Returns:
     In case of success, returns 0. There is no expected error case.
 */
static int ext2_release(const char *path, struct fuse_file_info *fi) {

  /* TO BE COMPLETED BY THE STUDENT */
  struct file_handler *fh = (struct file_handler *)fi->fh;
  if (fh->processor == 1){
    free(fh);
  } else{
    fh->processor -= 1;
  }
  return 0; // Function not implemented
}

/* ext2_read: Function called when a process reads data from a file in
   the file system. This function stores, in array 'buf', up to 'size'
   bytes from the file, starting at offset 'offset'. It may store less
   than 'size' bytes only in case of error or if the file is smaller
   than size+offset.
   
   Parameters:
     path: Path of the open file.
     buf: Pointer where data is expected to be stored.
     size: Maximum number of bytes to be read from the file.
     offset: Byte offset of the first byte to be read from the file.
     fi: Data structure containing information about the file being
         opened. This is the same structure used in ext2_open.
   Returns:
     In case of success, returns the number of bytes actually read
     from the file--which may be smaller than size, or even zero, if
     (and only if) offset+size is beyond the end of the file. In case
     of error, may return one of these error codes (not all of them
     are required):
       -ENOENT: If the file does not exist;
       -EISDIR: If the path corresponds to a directory;
       -EIO: If there was an I/O error trying to obtain the data.
 */
static int ext2_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi) {

  /* TO BE COMPLETED BY THE STUDENT */
  struct file_handler *fh = (struct file_handler *)fi->fh;
  ssize_t ret;
  if ((fh->inode.i_mode & S_IFMT) == S_IFDIR) {
    return -EISDIR; //the file is a directory
  }
  while(fh->lock){
    //locked
  }
  fh->lock = 1;
  ret = read_file_content(volume,&fh->inode,(uint64_t)offset,size,buf);
  fh->lock = 0;
  if (ret < 0)
    return -EIO;
  return (int)ret;
}

/* ext2_read: Function called when FUSE needs to obtain the target of
   a symbolic link. The target is stored in buffer 'buf', which stores
   up to 'size' bytes, as a NULL-terminated string. If the target is
   too long to fit in the buffer, the target should be truncated.
   
   Parameters:
     path: Path of the symbolic link file.
     buf: Pointer where symbolic link target is expected to be stored.
     size: Maximum number of bytes to be stored into the buffer,
           including the NULL byte.
   Returns:
     In case of success, returns 0 (zero). In case of error, may
     return one of these error codes (not all of them are required):
       -ENOENT: If the file does not exist;
       -EINVAL: If the path does not correspond to a symbolic link;
       -EIO: If there was an I/O error trying to obtain the data.
 */
static int ext2_readlink(const char *path, char *buf, size_t size) {

  /* TO BE COMPLETED BY THE STUDENT */
  uint32_t ret;
  inode_t inode;
  if (!find_file_from_path(volume, path, &inode)) {
    return -ENOENT; //Cannot find the file
  }else {
    if ((inode.i_mode & S_IFMT) != S_IFLNK) {
      return -EINVAL; //the file is not a symbolic link;
    }
    //Todo: When to return the IO error.
    uint64_t len = inode_file_size(volume,&inode);
    if(len <= sizeof(inode.i_symlink_target)){
      memcpy(buf,inode.i_symlink_target,size);
      if(size < sizeof(inode.i_symlink_target))
        buf[size - 1] = '\0';
    }else{
      ret = (uint32_t)read_file_content(volume,&inode,0,size,buf);
      if (ret < 0)
        return -EIO;
    }
  }
  return 0;
}
