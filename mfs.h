/**************************************************************
 * Class::  CSC-415-02 Spring 2024
 * Name:: Thiha Aung, Min Ye Thway Khaing, Dylan Nguyen
 * GitHub-Name:: thihaaung32
 * Group-Name:: Bee
 * Project:: Basic File System
 *
 * File:: mfs.h
 *
 * Description::
 *	This is the file system interface.
 *	This is the interface needed by the driver to interact with
 *	your filesystem.
 *
 **************************************************************/

#ifndef _MFS_H
#define _MFS_H
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "b_io.h"
#include "structure.h"
#include <dirent.h>
#include <sys/stat.h>

#define FT_REGFILE DT_REG
#define FT_DIRECTORY DT_DIR
#define FT_LINK DT_LNK

#ifndef uint64_t
typedef u_int64_t uint64_t;
#endif
#ifndef uint32_t
typedef u_int32_t uint32_t;
#endif

struct fs_diriteminfo
{
	unsigned short d_reclen; /* length of this record */
	unsigned char fileType;
	char d_name[256]; /* filename max filename is 255 characters */
};

typedef struct
{
	/*****TO DO:  Fill in this structure with what your open/read directory needs  *****/
	unsigned short d_reclen;		 /* length of this record */
	unsigned short dirEntryPosition; /* which directory entry position, like file pos */

	uint64_t directoryStartLocation;	/* To track DirectoryEntry location */
	struct fs_diriteminfo *di; /* Pointer to the structure you return from read */
	unsigned int current_index;			// current index for tracking readdir location
} fdDir;

// Returns a directory (an array of directory entries)
DirectoryEntry *parsePath(const char *path);

// Key directory functions
int fs_mkdir(const char *pathname, mode_t mode);
int fs_rmdir(const char *pathname);

// Directory iteration functions
fdDir *fs_opendir(const char *pathname);
struct fs_diriteminfo *fs_readdir(fdDir *dirp);
int fs_closedir(fdDir *dirp);

// Misc directory functions
char *fs_getcwd(char *pathname, size_t size);
int fs_setcwd(char *pathname); // linux chdir
int fs_isFile(char *filename); // return 1 if file, 0 otherwise
int fs_isDir(char *pathname);  // return 1 if directory, 0 otherwise
int fs_delete(char *filename); // removes a file

// This is the structure that is filled in from a call to fs_stat
struct fs_stat
{
	off_t st_size;		  /* total size, in bytes */
	blksize_t st_blksize; /* blocksize for file system I/O */
	blkcnt_t st_blocks;	  /* number of 512B blocks allocated */
	time_t st_accesstime; /* time of last access */
	time_t st_modtime;	  /* time of last modification */
	time_t st_createtime; /* time of last status change */

	/* add additional attributes here for your file system */
};

int fs_stat(const char *path, struct fs_stat *buf);

#endif