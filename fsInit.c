/**************************************************************
 * Class::  CSC-415-02 Spring 2024
 * Name:: Thiha Aung, Min Ye Thway Khaing, Dylan Nguyen
 * GitHub-Name:: thihaaung32
 * Group-Name:: Bee
 * Project:: Basic File System
 *
 * File:: fsInit.c
 *
 * Description:: Main driver for file system assignment.
 *
 * This file is where you will start and initialize your system
 *
 **************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#include "mfs.c"
#include "freeSpaceManagement.c"
#include "memo.c"

#define OUR_SIGNATURE 0x286D8A17


VCB *myVCB;
int *bitmap;
char *get_cwd;
DirectoryEntry *cw_dir_array;

// initialize volume control block
void initVCB()
{
  myVCB->signature = OUR_SIGNATURE;
  myVCB->mounting_time = time(NULL);
  strncpy(myVCB->volumeName, "MyVolume", sizeof(myVCB->volumeName) - 1);
}

// Initialize a root directory including "." , ".."
int initRootDirectory(int parent_location)
{
  // malloc a directory entry array
  int num_blocks = get_num_blocks(sizeof(DirectoryEntry) * DE_COUNT, myVCB->block_size);
  int numBytes = num_blocks * myVCB->block_size;
  DirectoryEntry *dirArray = malloc(numBytes);

  // allocate free space for the directory array
  int dir_location = allocateBlock(num_blocks);

  // Directory "." entry initialization
  dirArray[0].size = numBytes;
  dirArray[0].num_blocks = num_blocks;
  dirArray[0].location = dir_location;
  time_t curr_time = time(NULL);
  dirArray[0].timeCreated = curr_time;
  dirArray[0].timeLastModified = curr_time;
  dirArray[0].timeLastViewed = curr_time;
  dirArray[0].attributes = 'd';
  strcpy(dirArray[0].name, ".");

  // Parent directory ".." entry initialization
  if (parent_location == 0)
  {
    // track number of blocks root directory has
    myVCB->root_blocks = num_blocks;
    dirArray[1].size = numBytes;
    dirArray[1].num_blocks = num_blocks;
    dirArray[1].location = dir_location;
    dirArray[1].timeCreated = curr_time;
    dirArray[1].timeLastModified = curr_time;
    dirArray[1].timeLastViewed = curr_time;
  }
  else
  {
    // load the parent directory to receive parent directory information
    DirectoryEntry *parent_dir = malloc(numBytes);
    if (LBAread(parent_dir, num_blocks, parent_location) != num_blocks)
    {
      perror("LBAread failed when reading parent directory.\n");
      return -1;
    }
    dirArray[1].size = parent_dir->size;
    dirArray[1].num_blocks = num_blocks;
    dirArray[1].location = parent_location;
    dirArray[1].timeCreated = parent_dir->timeCreated;
    dirArray[1].timeLastModified = parent_dir->timeLastModified;
    dirArray[1].timeLastViewed = parent_dir->timeLastViewed;

    free(parent_dir);
    parent_dir = NULL;
  }

  dirArray[1].attributes = 'd';
  strcpy(dirArray[1].name, "..");

  // set all other directory entries to available
  for (int i = 2; i < DE_COUNT; i++)
  {
    dirArray[i].attributes = 'a';
  }

  // write new directory to disk
  int blocks_written = LBAwrite(dirArray, num_blocks, dir_location);

  if (blocks_written != num_blocks)
  {
    perror("LBAwrite failed\n");
    return -1;
  }

  free(dirArray);
  dirArray = NULL;

  // write updated free space to disk
  if (LBAwrite(bitmap, myVCB->freespace_size, 1) != myVCB->freespace_size)
  {
    perror("LBAwrite failed when trying to write the freespace\n");
  }

  return dir_location;
}

// helper function to receive the token in a directory entry array
int get_de_index(char *token, DirectoryEntry *dirArray)
{
  for (int i = 0; i < DE_COUNT; i++)
  {
    if (strcmp(token, dirArray[i].name) == 0)
    {
      return i;
    }
  }
  return -1;
}

// helper function to get the first available directory entry
int get_avail_de_idx(DirectoryEntry *dirArray)
{
  for (int i = 2; i < DE_COUNT; i++)
  {
    if (dirArray[i].attributes == 'a')
    {
      return i;
    }
  }
  perror("No available space in directory\n");
  return -1;
}

int initFileSystem(uint64_t numberOfBlocks, uint64_t blockSize)
{
  printf("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);

  // Allocate space for the Volume Control Block
  myVCB = malloc(blockSize);
  if (!myVCB)
  {
    perror("Failed to allocate VCB");
    return -1;
  }

  // the size needed for the bitmap and allocate
  int numBitmapBlocks = get_num_blocks(sizeof(int) * numberOfBlocks, blockSize);
  bitmap = malloc(numBitmapBlocks * blockSize);
  if (!bitmap)
  {
    perror("Failed to allocate bitmap");
    free(myVCB);
    return -1;
  }

  // Read VCB from the first block of the file system
  LBAread(myVCB, 1, 0);

  if (myVCB->magic == OUR_SIGNATURE)
  {
    // File system exists, load existing free space configuration
    if (load_free() != myVCB->freespace_size)
    {
      perror("Failed to load free space configuration");
      return -1;
    }
  }
  else
  {
    // No valid file system found, initialize a new one
    myVCB->magic = OUR_SIGNATURE;
    myVCB->blockTotal = numberOfBlocks;
    myVCB->block_size = blockSize;

    // Initialize the free space and root directory
    myVCB->fsLocation = initializeFreeSpace();
    if (myVCB->fsLocation == -1)
    {
      perror("Failed to initialize free space");
      return -1;
    }

    myVCB->rootDirLocation = initRootDirectory(0);
    if (myVCB->rootDirLocation == -1)
    {
      perror("Failed to initialize root directory");
      return -1;
    }
  }

  // Initialize the Volume Control Block
  initVCB();

  // Allocate and read the current working directory array
  cw_dir_array = malloc(myVCB->block_size * myVCB->root_blocks);
  if (!cw_dir_array)
  {
    perror("Failed to allocate cw_dir_array");
    return -1;
  }
  LBAread(cw_dir_array, myVCB->root_blocks, myVCB->rootDirLocation);

  // Allocate and set the path to root
  get_cwd = malloc(MAX_PATH_LENGTH);
  if (!get_cwd)
  {
    perror("Failed to allocate get_cwd");
    return -1;
  }
  strcpy(get_cwd, "/");

  printf("Free Space Management system initialized.\n");
  return 0;
}

void exitFileSystem()
{
  printf("Exiting File System...\n");

  flushAllBuffers();
  closeAllOpenFiles();
  writeBackMetadata();

  // Free allocated memory
  free(bitmap);

  free(myVCB);

  free(cw_dir_array);

  free(get_cwd);

  printf("Files system changes saved and exited clearly.\n");
}
