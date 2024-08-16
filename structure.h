/**************************************************************
 * Class::  CSC-415-02 Spring 2024
 * Name:: Thiha Aung, Min Ye Thway Khaing, Dylan Nguyen
 * GitHub-Name:: thihaaung32
 * Group-Name:: Bee
 * Project:: Basic File System
 *
 * File:: structure.c
 *
 * Description::  This file is for Directory Entry Structure and
 * 	Volume Control Block Structure for the whole File
 *	System Project.
 *
 **************************************************************/
#ifndef STRUCTURE_H
#define STRUCTURE_H
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include <dirent.h>

#ifndef uint64_t
typedef u_int64_t uint64_t;
#endif
#ifndef uint32_t
typedef u_int32_t uint32_t;
#endif

#define DE_COUNT 64				// initial number of d_entries to allocate to a directory
#define MAX_PATH_LENGTH 1024	// initial path length
#define DEFAULT_FILE_BLOCKS 128 // initial number of blocks for new files

// This is the directory entry structure for the file system
// This struct is exactly 128 bytes in size
typedef struct
{
	time_t timeCreated;		 // time file was created
	time_t timeLastModified; // time file was last modified
	time_t timeLastViewed;	 // time file was last accessed
	uint64_t location;		 // block location of file
	uint64_t size;			 // size of the file in bytes
	unsigned int num_blocks; // number of blocks

	char name[256]; // name of file
	unsigned char attributes; // attributes of file 
	
} DirectoryEntry;

// This is the volume control block structure for the file system
typedef struct
{
	int blockTotal;			 // number of blocks in the file system
	int block_size;			 // size of each block in the file system
	int fsLocation;			 // location of the first block of the freespace map
	int freeSpaceStartBlock; // reference to the first free block in the drive
	int freeBlocks;			 // number of blocks available in freespace
	int freespace_size;		 // number of blocks that freespace occupies
	int rootDirLocation;	 // block location of root
	int root_blocks;		 // number of blocks the root directory occupies
	long magic;				 // unique volume identifier
	uint64_t signature;		 // our signature
	time_t mounting_time;    
	char volumeName[256];
} VCB;

extern VCB *myVCB;					 // volume control block
extern int *bitmap;					 // freespace map
extern char *get_cwd;				 // get current working path string
extern DirectoryEntry *cw_dir_array; // directory structure 

int initRootDirectory(int parent_location);

int get_de_index(char *token, DirectoryEntry *dirArray);

int get_avail_de_idx(DirectoryEntry *dirArray);

#endif //STRUCTURE_H
