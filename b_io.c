/**************************************************************
 * Class::  CSC-415-02 Spring 2024
 * Name:: Thiha Aung, Min Ye Thway Khaing, Dylan Nguyen
 * GitHub-Name:: thihaaung32
 * Group-Name:: Bee
 * Project:: Basic File System
 *
 * File:: b_io.c
 *
 * Description:: Basic File System - Key File I/O Operations
 *
 **************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"
#include "mfs.h"
#include "fsLow.h"
#include "freeSpaceManagement.h"

#define MAXFCBS 20

// file control block buffer struct
typedef struct b_fcb
{
  char *buf;                // buffer for open file
  int index;                // hold the current position in buffer
  int bufLen;               // number of bytes in the buffer
  int currentBlk;           // current file system block location
  int numBlocks;            // current block index number
  int fileIndex;            // index of file in dirArray
  int accessMode;           // file access mode
  DirectoryEntry *fi;       // holds the low level systems file info
  DirectoryEntry *dirArray; // holds the directory array where the file resides

} b_fcb;

b_fcb fcbArray[MAXFCBS];

int startup = 0; // Indicates that this has not been initialized

// Method to initialize our file system
void b_init()
{
  // init fcbArray to all free
  for (int i = 0; i < MAXFCBS; i++)
  {
    fcbArray[i].buf = NULL; // indicates a free fcbArray
  }

  startup = 1;
}

// Method to get a free FCB element
b_io_fd b_getFCB()
{
  for (int i = 0; i < MAXFCBS; i++)
  {
    if (fcbArray[i].buf == NULL)
    {
      return i; // Not thread safe (But do not worry about it for this assignment)
    }
  }
  return (-1); // all in use
}

// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open(char *filename, int flags)
{
  b_io_fd returnFd;
  if (startup == 0)
    b_init(); // Initialize our system

  // parse path the dirrectory array
  DirectoryEntry *dirArray = parsePath(filename);

  char *last_token = get_last_token(filename);

  int found_index = get_de_index(last_token, dirArray);

  if (found_index < 0)
  {
    int error_occured = 0;

    if (flags & O_RDONLY)
    {
      perror("file not found\n");
      error_occured = 1;
    }

    if (!(flags & O_CREAT))
    {
      perror("new file cannot be created\n");
      error_occured = 1;
    }

    if (error_occured)
    {
      free(dirArray);
      dirArray = NULL;
      free(last_token);
      last_token = NULL;

      return -2;
    }
  }

  // allocate the file system buffer
  char *buf = malloc(myVCB->block_size);
  if (buf == NULL)
  {
    perror("b_open: buffer malloc failed\n");
    free(dirArray);
    dirArray = NULL;
    free(last_token);
    last_token = NULL;

    return (-1);
  }

  returnFd = b_getFCB(); // get our own file descriptor
                         // check for error - all used FCB's
  if (returnFd == -1)
  {
    perror("no free file control blocks available\n");

    free(dirArray);
    free(last_token);

    return -1;
  }

  // malloc file directory entry
  fcbArray[returnFd].fi = malloc(sizeof(DirectoryEntry));

  if (fcbArray[returnFd].fi == NULL)
  {
    perror("Malloc fcbArray file info failed\n");

    free(dirArray);
    free(last_token);

    return -1;
  }

  // if a file was found, load the directory entry. If not, create a new file

  if (found_index > -1)
  {
    memcpy(fcbArray[returnFd].fi, &dirArray[found_index], sizeof(DirectoryEntry));
    fcbArray[returnFd].fileIndex = found_index;
  }
  else
  {
    int new_index = get_avail_de_idx(dirArray);
    if (new_index == -1)
    {
      return -1;
    }

    int new_location = allocateBlock(DEFAULT_FILE_BLOCKS);
    if (new_location == -1)
    {
      return -1;
    }

    // New directory entry initialization
    dirArray[new_index].size = 0;
    dirArray[new_index].num_blocks = DEFAULT_FILE_BLOCKS;
    dirArray[new_index].location = new_location;
    time_t curr_time = time(NULL);
    dirArray[new_index].timeCreated = curr_time;
    dirArray[new_index].timeLastModified = curr_time;
    dirArray[new_index].timeLastViewed = curr_time;
    dirArray[new_index].attributes = 'f';
    strcpy(dirArray[new_index].name, last_token);

    // write new empty file to disk
    write_fs(dirArray);

    // copy new directory entry to fcbArray file info
    memcpy(fcbArray[returnFd].fi, &dirArray[new_index], sizeof(DirectoryEntry));

    fcbArray[returnFd].fileIndex = new_index;
  }

  // initialize fcbArray entry
  fcbArray[returnFd].dirArray = dirArray;
  fcbArray[returnFd].buf = buf;
  fcbArray[returnFd].index = 0;
  fcbArray[returnFd].bufLen = 0;
  fcbArray[returnFd].numBlocks = 0;
  fcbArray[returnFd].currentBlk = fcbArray[returnFd].fi->location;
  fcbArray[returnFd].accessMode = flags;

  // Per man page requirements, O_TRUNC sets file size to zero;
  if (flags & O_TRUNC)
  {
    fcbArray[returnFd].fi->size = 0;
  }

  free(last_token);
  last_token = NULL;

  return (returnFd);
}

// Interface to seek function
int b_seek(b_io_fd fd, off_t offset, int whence)
{
  if (startup == 0)
    b_init(); // Initialize our system

  // check that fd is between 0 and (MAXFCBS-1)
  if ((fd < 0) || (fd >= MAXFCBS))
  {
    return (-1); // invalid file descriptor
  }

  // Calculate block offset to retrieve the current block number
  int block_offset = offset / myVCB->block_size;

  // Set absolute block offset
  if (whence & SEEK_SET)
  {
    fcbArray[fd].currentBlk = get_block(fcbArray[fd].fi->location, block_offset);
  }

  // set relative block offset
  else if (whence & SEEK_CUR)
  {
    fcbArray[fd].currentBlk = get_block(fcbArray[fd].currentBlk, block_offset);
  }

  // set current block to the block following the end of file
  else if (whence & SEEK_END)
  {
    fcbArray[fd].currentBlk = get_block(fcbArray[fd].fi->location, fcbArray[fd].fi->num_blocks);
  }

  // set buffer offset to whatever remains after all the full blocks are
  // accounted for.
  fcbArray[fd].index = offset % myVCB->block_size;
  fcbArray[fd].bufLen = 0;
  fcbArray[fd].fi->timeLastViewed = time(NULL);

  return fcbArray[fd].index; // Change this
}

// Interface to write function
int b_write(b_io_fd fd, char *buffer, int count)
{
  if (startup == 0)
    b_init(); // Initialize our system

  // check that fd is between 0 and (MAXFCBS-1)
  if ((fd < 0) || (fd >= MAXFCBS) || count < 0)
  {
    return (-1); // invalid file descriptor
  }

  // check to see if the fcb exists in this location
  if (fcbArray[fd].fi == NULL)
  {
    return -1;
  }

  // calculate if extra blocks are necessary
  int extra_blocks = get_num_blocks(
      fcbArray[fd].fi->size + count + myVCB->block_size - (fcbArray[fd].fi->num_blocks * myVCB->block_size),
      myVCB->block_size);

  if (extra_blocks > 0)
  {
    // set the number of extra blocks
    extra_blocks = fcbArray[fd].fi->num_blocks > extra_blocks
                       ? fcbArray[fd].fi->num_blocks
                       : extra_blocks;

    // allocate the free blocks and save location
    int free_location = allocateBlock(extra_blocks);

    if (free_location < 0)
    {
      perror("Freespace allocation failed\n\n");
      return -1;
    }

    // set final block of file in the free space map to the starting block
    bitmap[get_block(fcbArray[fd].fi->location, fcbArray[fd].fi->num_blocks - 1)] = free_location;
    fcbArray[fd].fi->num_blocks += extra_blocks;
  }

  int bytesDelivered = 0;
  int avail_Bytes;
  if (fcbArray[fd].fi->size < 1)
  {
    fcbArray[fd].fi->size = 0;
    avail_Bytes = myVCB->block_size;
  }
  else
  {
    // available bytes in buffer
    avail_Bytes = myVCB->block_size - fcbArray[fd].index;
  }

  /* split buffer into three parts. part1 is what is left in the buffer,
   part2 is the section of the buffer that occupies zero to many entire blocks,
   part3 is the remaining amount to partially fill the buffer. */

  int part1, part2, part3, numBlocksToCopy, blocksWritten;
  if (avail_Bytes >= count)
  {
    part1 = count;
    part2 = 0;
    part3 = 0;
  }
  else
  {

    part1 = avail_Bytes;

    // set the part3 section to all the bytes left
    part3 = count - avail_Bytes;

    /*Divide Part3 by the chunk size to find the total blocks,
    then multiply by the chunk size for Part2's byte size. */
    numBlocksToCopy = part3 / myVCB->block_size;
    part2 = numBlocksToCopy * myVCB->block_size;

    // Subtract the complete bytes to get the part3 bytes left
    part3 = part3 - part2;
  }

  // part1 section
  if (part1 > 0)
  {
    // copy part1 of the user's buffer into the fcb buffer
    memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer, part1);

    // write the entire block to disk
    blocksWritten = LBAwrite(fcbArray[fd].buf, 1, fcbArray[fd].currentBlk);

    // set buffer offset
    fcbArray[fd].index += part1;

    // get the next block and reset the fcb buffer offset to zero.
    if (fcbArray[fd].index >= myVCB->block_size)
    {
      fcbArray[fd].currentBlk = get_next_block(fcbArray[fd].currentBlk);
      fcbArray[fd].index = 0;
    }
  }

  if (part2 > 0)
  {
    blocksWritten = 0;

    for (int i = 0; i < numBlocksToCopy; i++)
    {
      blocksWritten += LBAwrite(buffer + part1 + (i * myVCB->block_size), 1, fcbArray[fd].currentBlk);
      fcbArray[fd].currentBlk = get_next_block(fcbArray[fd].currentBlk);
    }
    part2 = blocksWritten * myVCB->block_size; // number of bytes written
  }

  if (part3 > 0)
  {
    // copy the user buffer into the fcb buffer
    memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer + part1 + part2, part3);

    // write entire block to disk
    blocksWritten = LBAwrite(fcbArray[fd].buf, 1, fcbArray[fd].currentBlk);

    fcbArray[fd].index += part3;
  }

  // if the fcb buffer is full, get the next block and set the offset to zero
  if (fcbArray[fd].index >= myVCB->block_size)
  {
    fcbArray[fd].currentBlk = get_next_block(fcbArray[fd].currentBlk);
    fcbArray[fd].index = 0;
  }

  // set accessed/modified times and file size
  time_t cur_time = time(NULL);
  fcbArray[fd].fi->timeLastViewed = cur_time;
  fcbArray[fd].fi->timeLastModified = cur_time;
  bytesDelivered = part1 + part2 + part3;
  fcbArray[fd].fi->size += bytesDelivered;

  // copy changes to the fcb directory entry
  memcpy(&fcbArray[fd].dirArray[fcbArray[fd].fileIndex], fcbArray[fd].fi, sizeof(DirectoryEntry));

  // write changes to disk
  write_dircetory(fcbArray[fd].dirArray);

  return part1 + part2 + part3;
}

// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+

int b_read(b_io_fd fd, char *buffer, int count)
{
  if (startup == 0)
    b_init(); // Initialize our system

  // check that fd is between 0 and (MAXFCBS-1)
  if ((fd < 0) || (fd >= MAXFCBS))
  {
    return -1; // invalid file descriptor
  }

  if (fcbArray[fd].accessMode & O_WRONLY)
  {
    perror("b_read: file does not have read access");
    return -1;
  }

  // check to see if the fcb exists
  if (fcbArray[fd].fi == NULL)
  {
    return -1;
  }

  int totalBytesRead = fcbArray[fd].numBlocks * myVCB->block_size + fcbArray[fd].index + 1;

  if (totalBytesRead >= fcbArray[fd].fi->size)
  {
    return -1;
  }

  // available bytes in buffer
  int avail_Bytes;

  avail_Bytes = fcbArray[fd].bufLen - fcbArray[fd].index;

  // number of bytes already delivered
  int bytesDelivered = (fcbArray[fd].numBlocks * myVCB->block_size) - avail_Bytes;

  // limit count to file length
  if ((count + bytesDelivered) > fcbArray[fd].fi->size)
  {
    count = fcbArray[fd].fi->size - bytesDelivered;
  }

  int part1, part2, part3, numBlocksToCopy, blocksRead;
  if (avail_Bytes >= count)
  {

    part1 = count;
    part2 = 0;
    part3 = 0;
  }
  else
  {
    part1 = avail_Bytes;

    // set the part3 section to what is left
    part3 = count - avail_Bytes;

    /*Divide Part3 by the chunk size to find the total blocks,
    then multiply by the chunk size for Part2's byte size. */
    numBlocksToCopy = part3 / myVCB->block_size;
    part2 = numBlocksToCopy * myVCB->block_size;

    // update the part3 bytes left
    part3 = part3 - part2;
  }

  // memcopy part1 section
  if (part1 > 0)
  {
    memcpy(buffer, fcbArray[fd].buf + fcbArray[fd].index, part1);

    fcbArray[fd].index += part1;
  }

  // LBAread all the complete blocks into the buffer
  if (part2 > 0)
  {
    blocksRead = 0;

    // read each block one-by-one since we don't know where the next block will be
    for (int i = 0; i < numBlocksToCopy; i++)
    {
      blocksRead += LBAread(buffer + part1 + (i * myVCB->block_size), 1, fcbArray[fd].currentBlk);
      fcbArray[fd].currentBlk = get_next_block(fcbArray[fd].currentBlk);
    }
    fcbArray[fd].numBlocks += blocksRead;
    part2 = blocksRead * myVCB->block_size;
  }

  // LBAread remaining block into the fcb buffer, and reset buffer offset
  if (part3 > 0)
  {
    blocksRead = LBAread(fcbArray[fd].buf, 1, fcbArray[fd].currentBlk);
    fcbArray[fd].bufLen = myVCB->block_size;

    fcbArray[fd].currentBlk = get_next_block(fcbArray[fd].currentBlk);
    fcbArray[fd].numBlocks += 1;
    fcbArray[fd].index = 0;

    // if the number of bytes is more than zero, copy the fd buffer to the buffer
    if (part3 > 0)
    {
      memcpy(buffer + part1 + part2, fcbArray[fd].buf + fcbArray[fd].index, part3);
      fcbArray[fd].index += part3;
    }
  }

  fcbArray[fd].fi->timeLastViewed = time(NULL);

  return part1 + part2 + part3;
}

// interface to move files or directories
int b_move(char *dest, char *src)
{

  DirectoryEntry *src_d_arr = parsePath(src);
  char *src_token = get_last_token(src);
  int src_index = get_de_index(src_token, src_d_arr);

  if (src_index < 0)
  {
    perror("file or directory not found");
    return -1;
  }

  DirectoryEntry *dir_path = parsePath(dest);
  char *dest_tok = get_last_token(dest);
  int dest_index = get_de_index(dest_tok, dir_path);

  if (dest_index > -1)
  {
    perror("file/directory with that name already exists");
    return -1;
  }

  if (dir_path[0].location == src_d_arr[0].location)
  {
    strcpy(src_d_arr[src_index].name, dest_tok);
    write_dircetory(src_d_arr);

    free(src_d_arr);
    src_d_arr = NULL;
    free(src_token);
    src_token = NULL;

    free(dir_path);
    dir_path = NULL;
    free(dest_tok);
    dest_tok = NULL;

    return 0;
  }

  // get an available directory entry
  dest_index = get_avail_de_idx(dir_path);

  if (dest_index < 0)
  {
    return -1;
  }

  // copy the source directory entry to the destination directory entry
  memcpy(&dir_path[dest_index], &src_d_arr[src_index], sizeof(DirectoryEntry));

  // Change the destination name, then write the changes to disk
  strcpy(dir_path[dest_index].name, dest_tok);
  write_dircetory(dir_path);

  // reset the source directory entry
  src_d_arr[src_index].name[0] = '\0';
  src_d_arr[src_index].attributes = 'a';
  write_dircetory(src_d_arr);

  free(src_d_arr);
  src_d_arr = NULL;
  free(src_token);
  src_token = NULL;

  free(dir_path);
  dir_path = NULL;
  free(dest_tok);
  dest_tok = NULL;

  return 0;
}

// Interface to Close the file
int b_close(b_io_fd fd)
{
  // write any changesto disk
  if (!(fcbArray[fd].accessMode & O_RDONLY) && fcbArray[fd].index > 0)
    LBAwrite(fcbArray[fd].buf, 1, fcbArray[fd].currentBlk);

  // copy changes to the fcb directory entry
  memcpy(&fcbArray[fd].dirArray[fcbArray[fd].fileIndex], fcbArray[fd].fi, sizeof(DirectoryEntry));

  write_fs(fcbArray[fd].dirArray);

  free(fcbArray[fd].dirArray);
  fcbArray[fd].dirArray = NULL;
  free(fcbArray[fd].fi);
  fcbArray[fd].fi = NULL;
  free(fcbArray[fd].buf);
  fcbArray[fd].buf = NULL;
}
