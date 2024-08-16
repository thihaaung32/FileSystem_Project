/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name:: Thiha Aung, Min Ye Thway Khaing, Dylan Nguyen
* GitHub-Name:: thihaaung32
* Group-Name:: Bee
* Project:: Basic File System
*
* File:: freeSpaceManagement.c
*
* Description:: Free Space Map Operations
*
**************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "freeSpaceManagement.h"
#include "fsLow.h"
#include "mfs.h"

// Initialize the freespace bitmap as specified in the file system volume control block.
int initializeFreeSpace()
{
  // Calculate the number of ints needed for the bitmap 
  int bitmap_size = (myVCB->blockTotal + 31) / 32; 
  bitmap = malloc(bitmap_size * sizeof(int)); // Allocate memory for the bitmap.
  if (!bitmap) {
    perror("Failed to allocate memory for freespace bitmap");
    return -1;
  }
  memset(bitmap, 0, bitmap_size * sizeof(int)); 

  // Mark the first block (block 0) as written in the bitmap.
  bitmap[0] |= 0x01;

  // Initialize VCB with freespace management.
  myVCB->freespace_size = bitmap_size;
  myVCB->freeBlocks = myVCB->blockTotal - 1; 

  // Write the updated bitmap to disk.
  int blocks_written = LBAwrite(bitmap, bitmap_size * sizeof(int), 1);
  if (blocks_written != bitmap_size * sizeof(int))
  {
    perror("LBAwrite failed\n");
    return -1;
  }

  return 1;  // Return success.
}

// Allocates the numberOfBlocks, and returns the first block of the allocation
int allocateBlock(int numberOfBlocks)
  {
  
  if (myVCB->freeBlocks < numberOfBlocks)
    {
    perror("Not enough freespace available.\n");   
    return -1;
    }
 
  int current_index = myVCB->freeSpaceStartBlock;
  int next = bitmap[myVCB->freeSpaceStartBlock];
  for (int i = 0; i < myVCB->freeBlocks - numberOfBlocks - 1; i++)
    {

    current_index = next;

    next = bitmap[next];
    }

  myVCB->freeBlocks = myVCB->freeBlocks - numberOfBlocks; // reduce available free space

  // return the index of the first block
  return next;
  }

// loads the free space map on the drive 
int load_free()
  {
  int readBlock = LBAread(bitmap, myVCB->freespace_size, 1);

  if (readBlock  != myVCB->freespace_size)
    {
    perror("LBAread failed to load momery.\n");
    return -1;
    }

  return readBlock;
  }

// get the block location from the block location provided to current position
int get_block(long location, int offset)
  {
  long current_location = location;
  long next = bitmap[current_location];
  for (int i = 0; i < offset; i++)
    {
    current_location = next;
    next = bitmap[next];
    }
  return current_location;
  }

// get the block location from the location provided
int get_next_block(long location)
  {
  return bitmap[location];
  }



int get_num_blocks(int bytes, int block_size)
  {
  return (bytes + block_size - 1)/(block_size);
  }

void write_fs(DirectoryEntry *dirArray)
  {
  // write all changes to disk
  if (LBAwrite(myVCB, 1, 0) != 1)
		{
		perror("LBAwrite failed when writing the VCB\n");
		}
	
	if (LBAwrite(bitmap, myVCB->freespace_size, 1) != myVCB->freespace_size)
		{
		perror("LBAwrite failed when writing the freespace\n");
		}
	
	if (LBAwrite(dirArray, dirArray[0].num_blocks, dirArray[0].location) != dirArray[0].num_blocks)
		{
		perror("LBAwrite failed when writing the directory\n");
		}

  if (dirArray[0].location == cw_dir_array[0].location)
    {
    memcpy(cw_dir_array, dirArray, dirArray[0].size);
    }
  }

void write_dircetory(DirectoryEntry *dirArray)
  {
  // write changes to directory to disk
	if (LBAwrite(dirArray, dirArray[0].num_blocks, dirArray[0].location) != dirArray[0].num_blocks)
		{
		perror("LBAwrite failed when writing the directory\n");
		}

  if (dirArray[0].location == cw_dir_array[0].location)
    {
    memcpy(cw_dir_array, dirArray, dirArray[0].size);
    }
  }


// sets the current working path
char *set_cwd()
  {
  // malloc DirectoryEntry array
  int num_blocks = get_num_blocks(sizeof(DirectoryEntry) * DE_COUNT, myVCB->block_size);
  int num_bytes = num_blocks * myVCB->block_size;
  DirectoryEntry *dirArray = malloc(num_bytes);

  int token_count = 0;
  // malloc an array of token pointers
  char **token_array = malloc(MAX_PATH_LENGTH);  

  // read the current working directory into memory
  LBAread(dirArray, cw_dir_array[0].num_blocks, cw_dir_array[0].location);

  if (dirArray[0].location == dirArray[1].location)
    {
    strcpy(get_cwd, "/");
    return get_cwd;
    }

  // load the current directory 
  int search_loc = dirArray[0].location;
  int path_size = 0;

  // iterate until we reach the root directory
  while (dirArray[0].location != dirArray[1].location)
    {
    // read the parent directory into memory 
    LBAread(dirArray, dirArray[1].num_blocks, dirArray[1].location);

    // iterate through the currently loaded directory to find the location
    for (int i = 2; i < DE_COUNT; i++)
      {
 
        if (dirArray[i].location == search_loc)
          {
          int name_size = strlen(dirArray[i].name) + 1;
          path_size += name_size;
          token_array[token_count] = malloc(name_size);
          strcpy(token_array[token_count++], dirArray[i].name);
          break;
          }
      }
    
    // set a new seach location as parent directory
    search_loc = dirArray[0].location;
    }

  char *path = malloc(path_size);

  strcpy(path, "/");
  for (int i = token_count - 1; i >= 0; i--)
    {
    strcat(path, token_array[i]);

    free(token_array[i]);
    token_array[i] = NULL;

    if (i > 0)
      strcat(path, "/");
    }

  strcpy(get_cwd, path);

  free(dirArray);
  dirArray = NULL;
  free(token_array);
  token_array = NULL;
  free(path);
  path = NULL;
  }

// helper function to get the last token from a path
char* get_last_token(const char *pathname)
  {
  char *path = malloc(strlen(pathname) + 1);
  strcpy(path, pathname);
  char *mallo_array = malloc(sizeof(char) * 128);
  char *last_token;
  char *token = strtok_r(path, "/", &last_token);  

  if (token == NULL)
    {
    strcpy(mallo_array, ".");
    return mallo_array;
    }

  while (token != NULL)
    {
    strcpy(mallo_array, token);
    token = strtok_r(NULL, "/", &last_token);
    }

  free(path);
  path = NULL;

  return mallo_array;
  }