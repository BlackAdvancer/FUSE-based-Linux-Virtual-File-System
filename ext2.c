#include "ext2.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fsuid.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>

#define EXT2_OFFSET_SUPERBLOCK 1024
#define EXT2_INVALID_BLOCK_NUMBER ((uint32_t) -1)

/* open_volume_file: Opens the specified file and reads the initial
   EXT2 data contained in the file, including the boot sector, file
   allocation table and root directory.
   
   Parameters:
     filename: Name of the file containing the volume data.
   Returns:
     A pointer to a newly allocated volume_t data structure with all
     fields initialized according to the data in the volume file
     (including superblock and group descriptor table), or NULL if the
     file is invalid or data is missing.
 */
volume_t *open_volume_file(const char *filename) {
  
  /* TO BE COMPLETED BY THE STUDENT */
  int fd = open(filename,O_RDONLY);
  int groupNum;
  int blockSize;
  if(fd < 0) return NULL;
  volume_t *volume = malloc(sizeof(struct ext2volume));

  //read the SuperBlock
  lseek(fd,EXT2_OFFSET_SUPERBLOCK,SEEK_SET);
  volume->fd = fd;
  read(fd,&volume->super, sizeof(superblock_t));

  //read the group descriptor
  groupNum = (volume->super.s_blocks_count)/(volume->super.s_blocks_per_group);
  blockSize = (uint32_t)(1024 << volume->super.s_log_block_size);
  if((volume->super.s_blocks_count)%(volume->super.s_blocks_per_group) != 0)
    groupNum++;
  group_desc_t *groups = malloc(groupNum * sizeof(group_desc_t));
  int location = (volume->super.s_first_data_block + 1) * blockSize;
  lseek(fd,location,SEEK_SET);
  for(int i = 0; i < groupNum; i++){
    read(fd,&groups[i],32 * sizeof(char));
  }

  //construct the volume
  volume->block_size = (uint32_t)blockSize;
  volume->volume_size = volume->block_size * volume->super.s_blocks_count;
  volume->num_groups = (uint32_t)groupNum;
  volume->groups = groups;

  return volume;
}

/* close_volume_file: Frees and closes all resources used by a EXT2 volume.
   
   Parameters:
     volume: pointer to volume to be freed.
 */
void close_volume_file(volume_t *volume) {

  /* TO BE COMPLETED BY THE STUDENT */
  close(volume->fd);
  free(volume->groups);
  free(volume);
}

/* read_block: Reads data from one or more blocks. Saves the resulting
   data in buffer 'buffer'. This function also supports sparse data,
   where a block number equal to 0 sets the value of the corresponding
\
 buffer to all zeros without reading a block from the volume.
   
   Parameters:
     volume: pointer to volume.
     block_no: Block number where start of data is located.
     offset: Offset from beginning of the block to start reading
             from. May be larger than a block size.
     size: Number of bytes to read. May be larger than a block size.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_block(volume_t *volume, uint32_t block_no, uint32_t offset, uint32_t size, void *buffer) {

  /* TO BE COMPLETED BY THE STUDENT */
  if(block_no == 0){
    memset(buffer,0,size);
    return size;   //Todo: what value should be returned.
  }
  int fd = volume->fd;
  uint32_t start = block_no * volume->block_size + offset;
  lseek(fd,start,SEEK_SET);
  ssize_t ret = read(fd,buffer,size);
  if(ret < 0)
    ret = -1;
  return ret;
}

/* read_inode: Fills an inode data structure with the data from one
   inode in disk. Determines the block group number and index within
   the group from the inode number, then reads the inode from the
   inode table in the corresponding group. Saves the inode data in
   buffer 'buffer'.
   
   Parameters:
     volume: pointer to volume.
     inode_no: Number of the inode to read from disk.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns a positive value. In case of error,
     returns -1.
 */
ssize_t read_inode(volume_t *volume, uint32_t inode_no, inode_t *buffer) {
  
  /* TO BE COMPLETED BY THE STUDENT */
  if(inode_no == 0)
    return -1;
  uint32_t blockGroup = (inode_no - 1)/volume->super.s_inodes_per_group;
  uint32_t localInodeIndex = (inode_no - 1) % volume->super.s_inodes_per_group;
  uint32_t inodeTable = volume->groups[blockGroup].bg_inode_table;
  uint32_t offset = localInodeIndex * volume->super.s_inode_size;
  return read_block(volume,inodeTable,offset,volume->super.s_inode_size,buffer);
}

/* read_ind_block_entry: Reads one entry from an indirect
   block. Returns the block number found in the corresponding entry.
   
   Parameters:
     volume: pointer to volume.
     ind_block_no: Block number for indirect block.
     index: Index of the entry to read from indirect block.

   Returns:
     In case of success, returns the block number found at the
     corresponding entry. In case of error, returns
     EXT2_INVALID_BLOCK_NUMBER.
 */
static uint32_t read_ind_block_entry(volume_t *volume, uint32_t ind_block_no,
				     uint32_t index) {
  
  /* TO BE COMPLETED BY THE STUDENT */
  uint32_t ret;
  if ( read_block(volume,ind_block_no,4 * index,4,&ret) == -1)
    return EXT2_INVALID_BLOCK_NUMBER;
  else
    return ret;   //Todo: Do I need to use the buffer or 32_uint.
}

/* read_inode_block_no: Returns the block number containing the data
   associated to a particular index. For indices 0-11, returns the
   direct block number; for larger indices, returns the block number
   at the corresponding indirect block.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure where data is to be sourced.
     index: Index to the block number to be searched.

   Returns:
     In case of success, returns the block number to be used for the
     corresponding entry. This block number may be 0 (zero) in case of
     sparse files. In case of error, returns
     EXT2_INVALID_BLOCK_NUMBER.
 */
static uint32_t get_inode_block_no(volume_t *volume, inode_t *inode, uint64_t block_idx) {
  
  /* TO BE COMPLETED BY THE STUDENT */
  uint32_t blockNum = EXT2_INVALID_BLOCK_NUMBER;
  uint32_t numBlock = volume->block_size/4;
  if(block_idx >= 0 && block_idx < 12){
    blockNum = inode->i_block[block_idx];
  }
  else if(block_idx - 12 < numBlock){
    uint32_t internalIdx = block_idx - 12;
    blockNum = read_ind_block_entry(volume,inode->i_block_1ind,internalIdx);
  }
  else if(block_idx - 12 < numBlock * numBlock){
    uint32_t internalIdx_1 = ((block_idx - 12 - numBlock) / numBlock);
    uint32_t  internalBlock = read_ind_block_entry(volume,inode->i_block_2ind,internalIdx_1);
    uint32_t internalIdx_2 = (block_idx - 12 - numBlock) % numBlock;
    blockNum = read_ind_block_entry(volume,internalBlock,internalIdx_2);
  }
  else if(block_idx - 12 < numBlock * numBlock * numBlock){
    uint32_t internalIdx_1 = ((block_idx - 12 - numBlock - numBlock * numBlock) / (numBlock * numBlock));
    uint32_t  internalBlock_1 = read_ind_block_entry(volume,inode->i_block_3ind,internalIdx_1);
    uint32_t internalIdx_2 = (block_idx - 12 - numBlock - numBlock * numBlock - internalIdx_1 * numBlock * numBlock) / numBlock;
    uint32_t internalBlock_2 = read_ind_block_entry(volume,internalBlock_1,internalIdx_2);
    uint32_t internalIdx_3 = (block_idx - 12 - numBlock - numBlock * numBlock - internalIdx_1 * numBlock * numBlock) % numBlock;
    blockNum = read_ind_block_entry(volume,internalBlock_2,internalIdx_3);
  }
  return blockNum;
}

/* read_file_block: Returns the content of a specific file, limited to
   a single block.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the file.
     offset: Offset, in bytes from the start of the file, of the data
             to be read.
     max_size: Maximum number of bytes to read from the block.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_file_block(volume_t *volume, inode_t *inode, uint64_t offset, uint64_t max_size, void *buffer) {
    
  /* TO BE COMPLETED BY THE STUDENT */
  uint64_t blcokIdx = offset/volume->block_size;
  uint32_t blcokOffset = offset % volume->block_size;
  uint32_t blockNum = get_inode_block_no(volume,inode,blcokIdx);
  if (blcokOffset + max_size > volume->block_size)
    max_size = volume->block_size - blcokOffset;
  return read_block(volume,blockNum,blcokOffset,max_size,buffer);
}

/* read_file_content: Returns the content of a specific file, limited
   to the size of the file only. May need to read more than one block,
   with data not necessarily stored in contiguous blocks.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the file.
     offset: Offset, in bytes from the start of the file, of the data
             to be read.
     max_size: Maximum number of bytes to read from the file.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_file_content(volume_t *volume, inode_t *inode, uint64_t offset, uint64_t max_size, void *buffer) {

  uint32_t read_so_far = 0;

  if (offset + max_size > inode_file_size(volume, inode))
    max_size = inode_file_size(volume, inode) - offset;
  
  while (read_so_far < max_size) {
    int rv = read_file_block(volume, inode, offset + read_so_far,
			     max_size - read_so_far, buffer + read_so_far);
    if (rv <= 0) return rv;
    read_so_far += rv;
  }
  return read_so_far;
}

/* follow_directory_entries: Reads all entries in a directory, calling
   function 'f' for each entry in the directory. Stops when the
   function returns a non-zero value, or when all entries have been
   traversed.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the directory.
     context: This pointer is passed as an argument to function 'f'
              unmodified.
     buffer: If function 'f' returns non-zero for any file, and this
             pointer is set to a non-NULL value, this buffer is set to
             the directory entry for which the function returned a
             non-zero value. If the pointer is NULL, nothing is
             saved. If none of the existing entries returns non-zero
             for 'f', the value of this buffer is unspecified.
     f: Function to be called for each directory entry. Receives three
        arguments: the file name as a NULL-terminated string, the
        inode number, and the context argument above.

   Returns:
     If the function 'f' returns non-zero for any directory entry,
     returns the inode number for the corresponding entry. If the
     function returns zero for all entries, or the inode is not a
     directory, or there is an error reading the directory data,
     returns 0 (zero).
 */
uint32_t follow_directory_entries(volume_t *volume, inode_t *inode, void *context,
				  dir_entry_t *buffer,
				  int (*f)(const char *name, uint32_t inode_no, void *context)) {

  /* TO BE COMPLETED BY THE STUDENT */
  //determine whether the Inode is a directory
  uint32_t ret = 0;
  if((inode->i_mode & S_IFMT) == S_IFDIR) {
    dir_entry_t entry;
    uint64_t fileSize = inode_file_size(volume, inode);
    uint32_t offset = 0;
    while (offset < fileSize) {
      read_file_content(volume, inode, offset, sizeof(dir_entry_t),&entry);
      if(entry.de_inode_no != 0) {
        char temp[256];
        strcpy(temp,entry.de_name);
       temp[entry.de_name_len] = '\0';//if the Inode is 0/NULL,skip it.
        if (f(temp, entry.de_inode_no, context) != 0) {
          ret = entry.de_inode_no;
          memcpy(buffer, &entry, sizeof(entry));
        }
      }
      offset += entry.de_rec_len;
    }
  }
  return ret;
}

/* Simple comparing function to be used as argument in find_file_in_directory function */
static int compare_file_name(const char *name, uint32_t inode_no, void *context) {
  return !strcmp(name, (char *) context);
}

/* find_file_in_directory: Searches for a file in a directory.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the directory.
     name: NULL-terminated string for the name of the file. The file
           name must match this name exactly, including case.
     buffer: If the file is found, and this pointer is set to a
             non-NULL value, this buffer is set to the directory entry
             of the file. If the pointer is NULL, nothing is saved. If
             the file is not found, the value of this buffer is
             unspecified.

   Returns:
     If the file exists in the directory, returns the inode number
     associated to the file. If the file does not exist, or the inode
     is not a directory, or there is an error reading the directory
     data, returns 0 (zero).
 */
uint32_t find_file_in_directory(volume_t *volume, inode_t *inode, const char *name, dir_entry_t *buffer) {
  
  return follow_directory_entries(volume, inode, (char *) name, buffer, compare_file_name);
}

/* find_file_from_path: Searches for a file based on its full path.
   
   Parameters:
     volume: Pointer to volume.
     path: NULL-terminated string for the full absolute path of the
           file. Must start with '/' character. Path components
           (subdirectories) must be delimited by '/'. The root
           directory can be obtained with the string "/".
     dest_inode: If the file is found, and this pointer is set to a
                 non-NULL value, this buffer is set to the inode of
                 the file. If the pointer is NULL, nothing is
                 saved. If the file is not found, the value of this
                 buffer is unspecified.

   Returns:
     If the file exists, returns the inode number associated to the
     file. If the file does not exist, or there is an error reading
     any directory or inode in the path, returns 0 (zero).
 */
uint32_t find_file_from_path(volume_t *volume, const char *path, inode_t *dest_inode) {

  /* TO BE COMPLETED BY THE STUDENT */
  dir_entry_t entry;
  inode_t inode;
  char temp[strlen(path) + 1];
  strcpy(temp,path);
  if(path[0] != '/')
    return 0;
  else if(strlen(path) == 1) {
    read_inode(volume,EXT2_ROOT_INO,dest_inode);
    return EXT2_ROOT_INO;
  }
  else{
    read_inode(volume,EXT2_ROOT_INO,&inode);
    char *fileName;
    fileName = strtok(temp,"/");
    while (fileName != NULL){
      if( find_file_in_directory(volume,&inode,fileName,&entry) == 0){
        return 0;
      }
      read_inode(volume,entry.de_inode_no,&inode);
      fileName = strtok(NULL,"/");
    }
    read_inode(volume,entry.de_inode_no,dest_inode);
  }
  return entry.de_inode_no;
}
