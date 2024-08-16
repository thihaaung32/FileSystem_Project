/**************************************************************
 * Class::  CSC-415-02 Spring 2024
 * Name:: Thiha Aung, Min Ye Thway Khaing, Dylan Nguyen
 * GitHub-Name:: thihaaung32
 * Group-Name:: Bee
 * Project:: Basic File System
 *
 * File:: mfs.c
 *
 * Description:: Operations of the file system interface needed by
 *            the driver to interact with the file system.
 *
 **************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "fsLow.h"
#include "freeSpaceManagement.h"

// Returns an array of directory entries
DirectoryEntry *parsePath(const char *path)
{

  char *pathname = malloc(strlen(path) + 1);
  strcpy(pathname, path);

  // malloc a directory entry array
  int num_blocks = get_num_blocks(sizeof(DirectoryEntry) * DE_COUNT, myVCB->block_size);
  int num_bytes = num_blocks * myVCB->block_size;
  DirectoryEntry *dirArray = malloc(num_bytes);

  /*if the path starts with '/', and the root directory must be loaded.*/
  if (pathname[0] == '/')
  {
    LBAread(dirArray, myVCB->root_blocks, myVCB->rootDirLocation);
  }
  else
  {
    memcpy(dirArray, cw_dir_array, num_bytes);
  }

  // malloc space for token array
  char *last_token;
  int token_counts = 0;
  char **token_array = malloc(strlen(pathname) * sizeof(char *));
  char *token = strtok_r(pathname, "/", &last_token);

  while (token != NULL)
  {
    token_array[token_counts++] = token;
    token = strtok_r(NULL, "/", &last_token);
  }

  // check if the directory exists through the token array.
  for (int i = 0; i < token_counts - 1; i++)
  {
    int found = get_de_index(token_array[i], dirArray);

    LBAread(dirArray, num_blocks, dirArray[found].location);
  }

  free(pathname);
  free(token_array);

  return dirArray;
}

// interface to get the current working directory
char *fs_getcwd(char *pathname, size_t size)
{
  
  return get_cwd;
}

// interface to set the current working directory:
int fs_setcwd(char *pathname)
{
  // if the pathname is the root directory, load the root directory
  if (strcmp(pathname, "/") == 0)
  {
    if (LBAread(cw_dir_array, myVCB->root_blocks, myVCB->rootDirLocation) != myVCB->root_blocks)
    {
      perror("LBAread failed when reading the directory\n");
    }

    set_cwd();

    return 0;
  }

  // load the directory of the path
  DirectoryEntry *dirArray = parsePath(pathname);

  if (dirArray == NULL)
  {
    printf("Invalid path: %s\n", pathname);
    return -1;
  }

  char *last_token = get_last_token(pathname);

  int found = get_de_index(last_token, dirArray);

  // if not found, or not a directory
  if (found == -1 || dirArray[found].attributes != 'd')
  {
    printf("No such file or directory with that name found.\n");
    return -1;
  }

  // read the directory into the current working directory array
  if (LBAread(cw_dir_array, dirArray[found].num_blocks,
              dirArray[found].location) != dirArray[found].num_blocks)
  {
    perror("LBAread failed when reading the directory\n");
  }

  set_cwd();

  return 0;
}

int fs_mkdir(const char *pathname, mode_t mode)
{
  // malloc pathname
  char *path = malloc(strlen(pathname) + 1);
  strcpy(path, pathname);

  // load the directory array
  DirectoryEntry *dirArray = parsePath(path);

  if (dirArray == NULL)
  {
    printf("Invalid path: %s\n", path);
    return -1;
  }

  // get last token of the path
  char *last_token = get_last_token(path);

  int found = get_de_index(last_token, dirArray);

  // check if a file/directory already exists
  if (found > -1)
  {
    printf("file or directory already exists\n");
    return -1;
  }

  // find an available directory entry location
  int new_index = get_avail_de_idx(dirArray);

  // no space left
  if (new_index == -1)
  {
    return -1;
  }

  // initialize a new directory as being the parent
  int new_location = initRootDirectory(dirArray[0].location);

  // calculate the number of blocks and bytes this directory occupies
  int num_blocks = get_num_blocks(sizeof(DirectoryEntry) * DE_COUNT, myVCB->block_size);
  int num_bytes = num_blocks * myVCB->block_size;

  // New directory entry initialization
  dirArray[new_index].size = num_bytes;
  dirArray[new_index].num_blocks = num_blocks;
  dirArray[new_index].location = new_location;
  dirArray[new_index].timeCreated = time(NULL);
  dirArray[new_index].timeLastModified = time(NULL);
  dirArray[new_index].timeLastViewed = time(NULL);
  dirArray[new_index].attributes = 'd';
  strcpy(dirArray[new_index].name, last_token);

  // write new directory to file system
  write_fs(dirArray);

  // free the malloc'd in functions
  free(dirArray);
  free(last_token);
  free(path);

  return new_location;
};

// remove directory interface
int fs_rmdir(const char *pathname)
{
  // malloc pathname
  char *path = malloc(strlen(pathname) + 1);
  strcpy(path, pathname);

  // load directory
  DirectoryEntry *dirArray = parsePath(path);

  char *last_token = get_last_token(path);

  int found = get_de_index(last_token, dirArray);

  // must exist and be a directory
  if (found < 2 || dirArray[found].attributes != 'd')
  {
    free(path);
    path = NULL;
    free(dirArray);
    dirArray = NULL;
    free(last_token);
    last_token = NULL;
    perror("fs_rmdir: remove directory failed.");
    return -1;
  }

  // reset the name, size, avalility and num_blocks
  dirArray[found].name[0] = '\0';
  dirArray[found].num_blocks = 0;
  dirArray[found].size = 0;
  dirArray[found].attributes = 'a';

  // write all changes to the file system to disk
  write_fs(dirArray);

  free(path);
  free(dirArray);
  free(last_token);

  return 0;
}

// delete file interface
int fs_delete(char *filename)
{
  
  char *path = malloc(strlen(filename) + 1);
  strcpy(path, filename);

  
  DirectoryEntry *dirArray = parsePath(path);

  char *last_token = get_last_token(path);

  int found = get_de_index(last_token, dirArray);

  // must exist and be a file
  if (found < 2 || dirArray[found].attributes != 'f')
  {
    free(path);
    path = NULL;
    free(dirArray);
    dirArray = NULL;
    free(last_token);
    last_token = NULL;
    perror("Delete file failed.\n");
    return -1;
  }

  // reset the name, size, space and num_blocks
  dirArray[found].name[0] = '\0';
  dirArray[found].num_blocks = 0;
  dirArray[found].size = 0;
  dirArray[found].attributes = 'a';

  // write all changes to the file system to disk
  write_fs(dirArray);

  free(path);
  path = NULL;
  free(dirArray);
  dirArray = NULL;
  free(last_token);
  last_token = NULL;

  return 0;
}

// isFile interface
int fs_isFile(char *filename)
{
  
  DirectoryEntry *dirArray = parsePath(filename);
  if (dirArray == NULL)
  {
    printf("Invalid File path.\n");
    return 0;
  }

  int found_entry = get_de_index(get_last_token(filename), dirArray);

  // must exist and be a file
  if (found_entry < 2 || dirArray[found_entry].attributes != 'f')
  {
    perror("File is not found\n");
    return 0;
  }
  return 1;
}

// isDir interface
int fs_isDir(char *pathname)
{
  DirectoryEntry *dirArray = parsePath(pathname);
  if (dirArray == NULL)
  {
    printf("Invalid directory path.\n");
    return 0;
  }
  int found_entry = get_de_index(get_last_token(pathname), dirArray);

  // must exist and be a directory
  if (found_entry < 0 || dirArray[found_entry].attributes != 'd')
  {
    return 0;
  }
  return 1;
}


int fs_stat(const char *path, struct fs_stat *buf)
{
  // malloc mutable path string
  char *pathname = malloc(strlen(path) + 1);
  strcpy(pathname, path);

  // load directory
  DirectoryEntry *dirArray = parsePath(pathname);
  if (dirArray == NULL)
  {
    printf("Invalid path: %s\n", pathname);
    return -1;
  }

  // set directory entry index
  int found_entry = get_de_index(get_last_token(pathname), dirArray);

  // Complete the fs_stat buffer
  buf->st_size = dirArray[found_entry].size;
  buf->st_blksize = myVCB->block_size;
  buf->st_blocks = dirArray[found_entry].num_blocks;
  buf->st_accesstime = dirArray[found_entry].timeLastViewed;
  buf->st_modtime = dirArray[found_entry].timeLastModified;
  buf->st_createtime = dirArray[found_entry].timeCreated;

  // dirArray needs to be freed
  free(dirArray);
  dirArray = NULL;

  return found_entry;
}

// open directory interface
fdDir *fs_opendir(const char *pathname)
{
  
  char *path = malloc(strlen(pathname) + 1);
  strcpy(path, pathname);


  DirectoryEntry *dirArray = parsePath(path);

  char *last_token = get_last_token(path);

  int found = get_de_index(last_token, dirArray);

  // check if a directory exists or not
  if (found < 0 || dirArray[found].attributes != 'd')
  {
    printf("Open directory failed.");
    return NULL;
  }

  // malloc file descriptor
  fdDir *fdDir_arr = malloc(sizeof(fdDir));

  // Complete the information from the index found for directory
  fdDir_arr->d_reclen = dirArray[found].num_blocks;
  fdDir_arr->dirEntryPosition = found;
  fdDir_arr->directoryStartLocation = dirArray[found].location;
  fdDir_arr->current_index = 0;

  struct fs_diriteminfo *di = malloc(sizeof(struct fs_diriteminfo));
  fdDir_arr->di = di;
  strcpy(fdDir_arr->di->d_name, last_token);
  fdDir_arr->di->d_reclen = dirArray[found].num_blocks;
  fdDir_arr->di->fileType = dirArray[found].attributes;

  free(path);
  path = NULL;
  free(dirArray);
  dirArray = NULL;
  free(last_token);
  last_token = NULL;

  return fdDir_arr;
}

// read directory interface, and return the current directory
struct fs_diriteminfo *fs_readdir(fdDir *dirp)
{
  if (dirp == NULL)
  {
    perror("Read directory failed.\n");
    return NULL;
  }

  // malloc directory array and load into memory
  int num_blocks = get_num_blocks(sizeof(DirectoryEntry) * DE_COUNT, myVCB->block_size);
  int num_bytes = num_blocks * myVCB->block_size;
  DirectoryEntry *dirArray = malloc(num_bytes);
  LBAread(dirArray, num_blocks, dirp->directoryStartLocation);

  while (dirArray[dirp->current_index].attributes == 'a' && dirp->current_index < DE_COUNT - 1)
  {
    dirp->current_index++;
  }

  // return NULL if not found the current item index
  if (dirp->current_index == DE_COUNT - 1)
  {
    free(dirArray);
    dirArray = NULL;
    return NULL;
  }

  // fill in the information for the directory item info as appropriate
  strcpy(dirp->di->d_name, dirArray[dirp->current_index].name);
  dirp->di->d_reclen = dirp->d_reclen;
  dirp->di->fileType = dirArray[dirp->current_index].attributes;

  dirp->current_index++;

  free(dirArray);
  dirArray = NULL;

  return dirp->di;
}

// fs_closedir close the directory of the file system
int fs_closedir(fdDir *dirp)
{
  if (dirp == NULL)
  {
    perror("Closeing directory failed.\n");
    return 0;
  }

  free(dirp->di);
  dirp->di = NULL;
  free(dirp);
  dirp = NULL;

  return 1;
}