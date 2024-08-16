/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name:: Thiha Aung, Min Ye Thway Khaing, Dylan Nguyen
* GitHub-Name:: thihaaung32
* Group-Name:: Bee
* Project:: Basic File System
*
* File:: freeSpaceManagement.h
*
* Description:: Interface for Free Space Map Functions
*
**************************************************************/

#ifndef _FREE_SPACE_MANAGEMENT_H
#define _FREE_SPACE_MANAGEMENT_H

#include "mfs.h"

int initializeFreeSpace();
int allocateBlock(int numberOfBlocks);
int load_free();
int get_block(long location, int offset);
int get_next_block(long location);

int get_num_blocks(int bytes, int block_size);

// Ensure the file changes are written back to disk
void write_fs(DirectoryEntry *dirArray);

// write changes of directory arry to disk
void write_dircetory(DirectoryEntry *dirArray);

char *set_cwd();

char* get_last_token(const char *pathname);

#endif