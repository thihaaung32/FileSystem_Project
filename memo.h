// memo.h
#ifndef MEMO_H
#define MEMO_H

#include <stdbool.h>  

#define BLOCK_SIZE 512  // Define according to your disk block size
#define MAX_BUFFERS 1024  // Maximum number of buffers in the buffer cache
#define MAX_OPEN_FILES 128

// Buffer structure definition
typedef struct {
    char data[BLOCK_SIZE];
    int blockNumber;
    bool dirty;
} Buffer;

//File descriptor structure definition
typedef struct{
    bool isOpen;
    FILE *file;
    long position;

} FileDescriptor;


// Function prototypes
extern FileDescriptor fileDescriptors[MAX_OPEN_FILES];

void initBuffers();
void flushAllBuffers();
void writeBlockToDisk(int blockNumber, const char* data);
void diskDeviceWrite(int blockNumber, const char* data, size_t size);
void closeAllOpenFiles();
void writeBackMetadata();  


#endif // MEMO_H
