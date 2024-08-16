#include "memo.h"
#include <stdio.h>
#include <string.h>
#include "structure.h"

// Global buffer cache
Buffer buffers[MAX_BUFFERS];

// Initialize the buffer cache
void initBuffers() {
    for (int i = 0; i < MAX_BUFFERS; i++) {
        buffers[i].dirty = false;
        buffers[i].blockNumber = -1;  // Indicates that the buffer is initially unused
    }
}


// Flush all buffers in the buffer cache
void flushAllBuffers() {
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (buffers[i].dirty) {
            writeBlockToDisk(buffers[i].blockNumber, buffers[i].data);
            buffers[i].dirty = false;  // Clear the dirty flag
        }
    }
    printf("All buffers have been flushed.\n");
}

void writeBackMetadata() {
    
    printf("Writing back metadata to the disk...\n");
    flushAllBuffers();  // Ensure all modified buffers are written back
}

// Write a block of data to the disk
void writeBlockToDisk(int blockNumber, const char* data) {
    // Ensure block size is dynamically pulled from VCB
    diskDeviceWrite(blockNumber, data, myVCB->block_size);
    printf("Block %d written to disk.\n", blockNumber);
}

// Disk write operation that writes data to a physical file (simulated disk block)
void diskDeviceWrite(int blockNumber, const char* data, size_t size) {
    char filename[30];
    sprintf(filename, "diskBlock_%d.bin", blockNumber);
    FILE *file = fopen(filename, "wb");
    if (file) {
        fwrite(data, size, 1, file);
        fclose(file);
    } else {
        printf("Error writing to disk block %d\n", blockNumber);
    }
}

// Close all open files
FileDescriptor fileDescriptors[MAX_OPEN_FILES];

void closeAllOpenFiles() {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (fileDescriptors[i].isOpen) {
            fclose(fileDescriptors[i].file);  // Close the file
            fileDescriptors[i].isOpen = 0;    // Mark as not in use
        }
        }
    }
