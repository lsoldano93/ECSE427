// ECSE 427 Assignment #3 - File System
// Luke Soldano, 260447714 (C) 2015

// ---------------------------------- Parameters -------------------------------------------------------

// Library inclusions
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "disk_emu.h"
#include "sfs_api.h"

// Limit definitions to abide by restrictions
#define MAX_FNAME_LENGTH 20 		// Max file name length - Chars
#define MAX_EXT_LENGTH 4   	    	// Max file extension length - Chars
#define MAX_FILES 99			// Max number of files that can be open
#define MAX_INODES MAX_FILES + 1	// Max number of iNodes is max files plus root iNode
#define DISK "Emulated_Disk.img"	// Name of the emulated disk
#define MAGIC_NUMBER 0xAABB0005 	// Magic number
#define BLOCK_SIZE 512			// Size of memory blocks - Bytes
#define DISK_SIZE 500			// Size of emulated disk - Blocks
#define INODE_SIZE 4*18			// Size of an iNode - Bytes
#define INODE_TABLE_SIZE ceil((MAX_INODES*INODE_SIZE)/BLOCK_SIZE) // Size of iNode Table - Blocks
#define NUMBER_INDIRECT_PTRS BLOCK_SIZE / 4 // Number of Indirect Ptrs stored within a block	
#define SUPER_BLOCK_ADDRESS 0       	// Beginning block address of the super block
#define INODE_TABLE_ADDRESS 1		// Beginning block address of the iNode table
#define DATA_BLOCKS_ADDRESS INODE_TABLE_SIZE + 1  // Beginning block address of data blocks
#define INBMAP_ADDRESS DISK_SIZE - 2 	// Beginning block address of the iNode bitmap
#define DBBMAP_ADDRESS DISK_SIZE - 1 	// Beginning block address of the data block bitmap
// Number of data blocks should be less than block size
#define NUMBER_DATA_BLOCKS INBMAP_ADDRESS - DATA_BLOCKS_ADDRESS // Number of total data blocks on the disk
#define INODES_PER_BLOCK BLOCK_SIZE / INODE_SIZE     // How many iNodes can be anticipated per iNT block
#define DIRENTRIES_PER_BLOCK BLOCK_SIZE / (MAX_FNAME_LENGTH + sizeof(int)) // How many dir entries per block
#define MAX_FSIZE BLOCK_SIZE*(12+NUMBER_INDIRECT_PTRS)	// Max file size - Bytes

// Define boolean
typedef int bool;
#define true 1
#define false 0

// Define super block
typedef struct superBlock_t{
	int magicNum; 			// 0xAABB0005
	int blockSize; 			// Should be 512 
	int fileSysSize;		// Number of blocks
	int iNodeTableLength;		// Number of blocks
	int rootDir;			// iNode number

} superBlock_t;

// Define i-Node
typedef struct iNode_t{
	int mode;    // This is never used by my sfs
	int linkCnt; // This is never used by my sfs
	int uid;     // This is never used by my sfs
	int gid;     // This is never used by my sfs
	int size;
	int directPtrs[12];
	int indirectPtr;
	int indirectPtrCnt;
	
} iNode_t;

typedef struct dirEntry_t{
	char fileName[MAX_FNAME_LENGTH + 1 + MAX_EXT_LENGTH]; // TODO, do u add 1 for '.'?
	int iNodeNumber;
} dirEntry_t;

typedef struct fdEntry_t{
	int iNodeNumber;
	int accessMode;
	int fileLoc;
	iNode_t* iNodePtr;
	
} fdEntry_t;

// ------------------------------ Global Variables -----------------------------------------------------

dirEntry_t dirEntryTable[MAX_FILES];
fdEntry_t fdTable[MAX_FILES];

int dirEntryIndex = 0;

// ------------------------------ Helper Functions -----------------------------------------------------

// Read iNode from the iNode table or write iNode to the iNode table ****************************
iNode_t* rwINodeTable(int iNodeNumber, iNode_t* iNodePtr, bool write){

	iNode_t* returnedINodePtr;
	iNode_t* iNodeBlock;

	char buffer[BLOCK_SIZE];

	int blockNumber = 0;
	int blockSpot = 0;

	memset(buffer, 0, BLOCK_SIZE);

	// Determine the block number and index of iNode entry
	//// Integer division will ensure that the correct block is selected
	blockNumber = iNodeNumber / INODES_PER_BLOCK;
	blockSpot = iNodeNumber % INODES_PER_BLOCK;
	
	// Read entire iNode block to memory
	read_blocks(INODE_TABLE_ADDRESS + blockNumber, 1, buffer);

	// Cast buffer as an iNode so that it can be used to read/write to the iNode/disk properly
	iNodeBlock = (iNode_t*) buffer;

	if (write){
		// Copy value of given iNode to proper location in its iNode block then write to disk
		//// TODO: After so many calls of this function, a segmentation fault eventually occurs
		printf("DEBUG: block num: %d, block spot: %d\n", blockNumber, blockSpot);
		memcpy(&iNodeBlock[blockSpot], iNodePtr, sizeof(iNode_t));
		write_blocks(INODE_TABLE_ADDRESS + blockNumber, 1, buffer);

		return iNodePtr;
	}
	else {
		// Initialize space for the desired iNode and copy it from disk to memory
		returnedINodePtr = malloc(sizeof(iNode_t));
		memcpy(returnedINodePtr, &iNodeBlock[blockSpot], sizeof(iNode_t));

		return returnedINodePtr;
	}
}

// Find free iNode ******************************************************************************
int findFreeINode(){

	int iNodeNumber = 0;
	
	char buffer[BLOCK_SIZE];

	memset(buffer, 0, BLOCK_SIZE);

	// Read iNode bit map into buffer
	read_blocks(INBMAP_ADDRESS, 1, buffer);

	// Scan for unused iNode in iNode bitmap
	for (int i=0; i < MAX_INODES; i++){

		// Save spot, mark as taken, and exit for loop if free spot found
		if (buffer[i] != 1){
			iNodeNumber = i;
			buffer[i] = 1;
			write_blocks(INBMAP_ADDRESS, 1, buffer);
			break;
		}

		// If every iNode taken and no free spot, return error
		else if (buffer[i] == 1 && i == MAX_INODES - 1){
			iNodeNumber = -1;
		}
	}

	return iNodeNumber;
}

// Find iNode pointer given iNode number ********************************************************
iNode_t* findINodePtrForGivenNumber(int iNodeNumber){

	iNode_t* iNodePtr;
	iNode_t* iNodeBlock;

	int blockNumber = 0;
	int blockSpot = 0;

	char buffer[BLOCK_SIZE];

	memset(buffer, 0, BLOCK_SIZE);

	iNodePtr = calloc(1, sizeof(iNode_t));

	blockNumber = iNodeNumber / BLOCK_SIZE;
	blockSpot = iNodeNumber % BLOCK_SIZE;
	
	// Read entire iNode block to memory
	read_blocks(INODE_TABLE_ADDRESS + blockNumber, 1, buffer);

	// Cast buffer as an iNode so that it can be used to read from the disk properly
	iNodeBlock = (iNode_t*) buffer;

	// Copy value from buffer
	memcpy(iNodePtr, &iNodeBlock[blockSpot], sizeof(iNode_t));	

	return iNodePtr;
}

// Find iNode using path argument passed to sfs_api *********************************************
int findINodeNumForGivenPath(const char* path){

	// See if iNode for path already exists
	for (int i=0; i < MAX_FILES; i++){
		if (strcmp(dirEntryTable[i].fileName, path) == 0){
			return i;
		}
	}
	
	// Return -1 if iNode doesn't exist yet
	return -1;

}

// Find a free file descriptor ID to place a file into ******************************************
int findFreeFd(){

	// Search for free space and set as open once found, else return error
	for (int i=0; i < MAX_FILES; i++){
		if (fdTable[i].accessMode == 0){
			fdTable[i].accessMode = 1;
			return i;
		}
	}

	return -1;
}

// Find a free block ****************************************************************************
int findFreeBlock(){

	int freeBlock;

	char buffer[BLOCK_SIZE];

	memset(buffer, 0, BLOCK_SIZE);

	// Read data blocks bitmap into buffer
	read_blocks(DBBMAP_ADDRESS, 1, buffer);

	// Save first free data block, otherwise if full return -1
	for (int i=0; i < NUMBER_DATA_BLOCKS; i++){
		if (buffer[i] == 0){
			freeBlock = i;
			break;

		}
		else if (i == NUMBER_DATA_BLOCKS - 1){
			return -1;

		}
	}

	// Mark block as in use in file system and save to disk
	buffer[freeBlock] = 1;
	write_blocks(DBBMAP_ADDRESS, 1, buffer);

	// Return correct address
	freeBlock = DATA_BLOCKS_ADDRESS + freeBlock;
	return freeBlock;
}
  
// -------------------------------- API Functions ------------------------------------------------------

// Creates the file system **********************************************************************
void mksfs(int fresh){

	superBlock_t superBlock;
	iNode_t rootINode;

	int returnCode = 0;
	int adrs = 0;

	char buffer[BLOCK_SIZE];

	// Set blocks to 0
	memset(buffer, 0, BLOCK_SIZE);

	// Create a new directory
	if (fresh){

		// Call disk_emu.c to create a new emulated disk
		returnCode = init_fresh_disk(DISK, BLOCK_SIZE, DISK_SIZE);
		if (returnCode != 0){
			printf("mksfs error 1: returned '%d', exiting code\n\n", returnCode);
			exit(0);
		}

		// Clear and initalize values of fdTable and dirEntry table
		for (int i=0; i < MAX_FILES; i++){
			dirEntryTable[i].iNodeNumber = 0;
			fdTable[i].iNodeNumber = 0;
			fdTable[i].accessMode = 0;
			fdTable[i].fileLoc = 0;
			fdTable[i].iNodePtr = 0;
		}

		// Setup super block
		//// Give basic parameters
		superBlock.magicNum = MAGIC_NUMBER;
		superBlock.blockSize = BLOCK_SIZE;
		superBlock.fileSysSize = DISK_SIZE;
		superBlock.iNodeTableLength = INODE_TABLE_SIZE;
		superBlock.rootDir = 0;
		//// Allocate 512 bytes (1 block) of space for super block at initial address of disk
		write_blocks(SUPER_BLOCK_ADDRESS, 1, (void*) &superBlock);
		
		// Setup iNode bitmap
		//// Mark root iNode as taken in iNode bitmap
		buffer[0] = 1;
		//// Allocate 1 block of space for iNode bitmap at correct address
		write_blocks(INBMAP_ADDRESS, 1, buffer); 

		// Setup data block bitmap (reserve first five blocks for dirEntryTable
		buffer[0] = 1;
		buffer[1] = 1;
		buffer[2] = 1;
		buffer[3] = 1;
		buffer[4] = 1;

		//// Allocate 1 block of space for data block bitmap at correct address
		write_blocks(DBBMAP_ADDRESS, 1, buffer); 
		
		// Ensure that the status of every file descriptor is unused to avoid issues
		for (int i=0; i < MAX_FILES; i++){
			fdTable[i].accessMode = 0;
		}

		// Initialize values of root iNode
		rootINode.mode = 0x1FF;		// 0001 1111 1111 - Permissions	
		rootINode.linkCnt = 1;		// Being pointed at by super block
		rootINode.uid = 0;		// Superuser ID
		rootINode.gid = 0;		// Superuser ID
		rootINode.size = 0;
		adrs = DATA_BLOCKS_ADDRESS;	// Initial address is StdO, next is StdI, next is StdE
		// Need first 5 data blocks for dirEntryTable
		memcpy(rootINode.directPtrs, (int [12]) {adrs,adrs+1,adrs+2,adrs+3,adrs+4,0,0,0,0,0,0,0}, 12*sizeof(int)); 
		rootINode.indirectPtr = 0;
		rootINode.indirectPtrCnt = 0;
	
		// Setup root directory in file descriptor table
		fdTable[0].iNodeNumber = 0;	// Root iNode 
		fdTable[0].accessMode = 1;
		fdTable[0].fileLoc = 0;		// Beginning of file
		fdTable[0].iNodePtr = &rootINode;

		// Write root iNode to disk
		rootINode = *rwINodeTable(0, fdTable[0].iNodePtr, true);


	} // End mksfs(fresh)

	// Reuse an existing directory
	else {

		// Call disk_emu.c to open existing emulated disk
		returnCode = init_disk(DISK, BLOCK_SIZE, DISK_SIZE);
		if (returnCode != 0){
			printf("mksfs error 2: returned '%d', exiting code\n\n", returnCode);
			exit(0);
		}

		// Get back up to speed by reinitiating values of root directory in file descriptor table
		fdTable[0].iNodeNumber = 0;	// Root iNode 
		fdTable[0].accessMode = 1;
		//// Read root iNode from the disk
		fdTable[0].iNodePtr = rwINodeTable(0, NULL, false);
		//// Place rw pointer at end of the file
		fdTable[0].fileLoc = fdTable[0].iNodePtr->size;		

	} // End mksfs(NOT fresh)

	return;
} // End mksfs()


// Get the name of the next file in the directory, return 1 if new file, 0 if complete **********
int sfs_getnextfilename(char *fname){

	while (dirEntryIndex <= MAX_FILES){

		// Complete and exit loop, restore values for next call
		if (dirEntryIndex == MAX_FILES){
			dirEntryIndex = 0;

		}

		// Return file name if file exists
		else {
			if (dirEntryTable[dirEntryIndex].iNodeNumber != 0){
				strcpy(fname, dirEntryTable[dirEntryIndex].fileName);	
				dirEntryIndex++;			
				return 1;

			}
	
		dirEntryIndex++;

		}

	}

	return 0;

} // End sfs_getnextfilename()


// Get the size of the given file ***************************************************************
int sfs_getfilesize(const char* path){

	iNode_t* iNodePtr;

	int size = 0;
	int iNodeNumber = 0;

	// Find the iNode corresponding to the inputted path
	iNodeNumber = findINodeNumForGivenPath(path);
	if (iNodeNumber == -1){
		printf("sfs_getfilesize error 1: no iNode found for specified path\n\n");
		return -1;
	}

	// Read the iNode with the given iNodeNumber to find size
	iNodePtr = rwINodeTable(iNodeNumber, NULL, false);
	size = iNodePtr->size; 
	
	// Free iNode from memory
	free(iNodePtr);

	return size;
} // End sfs_getfilesize()


// Opens the given file, returns index to file in fdTable ***************************************
int sfs_fopen(char *name){

	iNode_t* iNodePtr;
	dirEntry_t dirEntry;
	dirEntry_t* dirEntryBlock;

	bool fileExists = false;

	int iNodeNumber = 0;
	int blockNumber = 0;
	int blockSpot = 0;
	int fileID = 0;

	char buffer[BLOCK_SIZE];

	memset(buffer, 0, BLOCK_SIZE);

	// Make space for an iNode in case one is needed for new file
	iNodePtr = calloc(1, sizeof(iNode_t));

	// Find the iNode corresponding to the inputted path
	iNodeNumber = findINodeNumForGivenPath(name);
	
	// See if file already open, only execute if file exists
	if (iNodeNumber != -1){
		for (int i=0; i < MAX_FILES; i++){
			if (fdTable[i].iNodeNumber == iNodeNumber){
				if (fdTable[i].accessMode == 1){
					printf("\n**********************************************************\n");     
					printf("File %s, was already open", name);
					printf("\n**********************************************************\n"); 
					return i;
				}
				else {
					fileExists = true;
					break;
				}
			} 
		}
	}

	// Find a file descriptor block to open file into
	fileID = findFreeFd();
	if (fileID == -1){
		printf("sfs_fopen error 2: file descriptor table is full\n\n");
		return -1;
	}

	// Does file exist?
	if (fileExists){
		// Assign iNodeNumber to fd, mark file as open, put rw pointer to end of file
		iNodePtr = findINodePtrForGivenNumber(iNodeNumber);
		fdTable[fileID].iNodeNumber = iNodeNumber;
		fdTable[fileID].iNodePtr = iNodePtr;
		fdTable[fileID].fileLoc = fdTable[fileID].iNodePtr->size;
		fdTable[fileID].accessMode = 1;

		printf("\n**********************************************************\n");    
		printf("Opening file %s, file already existed", name);
		printf("\n**********************************************************\n"); 
		
	}

	// File doesn't exist yet
	else {
		// Find an iNode for file
		iNodeNumber = findFreeINode();

		// Initialize iNode values
		iNodePtr->size = 0;
		iNodePtr->indirectPtr = 0;
		iNodePtr->indirectPtrCnt = 0;
		memcpy(iNodePtr->directPtrs, (int [12]) {0,0,0,0,0,0,0,0,0,0,0,0}, 12*sizeof(int));

		// Write iNode to disk
		iNodePtr = rwINodeTable(iNodeNumber, iNodePtr, true);

		// Assign iNodeNumber to fd, mark file as open, put rw pointer to end of file
		fdTable[fileID].iNodeNumber = iNodeNumber;
		fdTable[fileID].iNodePtr = iNodePtr;
		fdTable[fileID].fileLoc = fdTable[fileID].iNodePtr->size;
		fdTable[fileID].accessMode = 1;

		// Create accurate directory entry and submit to dirEntry table
		dirEntry.iNodeNumber = iNodeNumber;
		strcpy(dirEntry.fileName, name);
		dirEntryTable[fileID] = dirEntry;

		// Write directory entry to disk
		blockNumber = (fileID/DIRENTRIES_PER_BLOCK) + DATA_BLOCKS_ADDRESS;
		blockSpot = fileID % BLOCK_SIZE;
		
		read_blocks(blockNumber, 1, buffer);
		dirEntryBlock = (dirEntry_t*) buffer;
		memcpy(&buffer[blockSpot], dirEntryBlock, sizeof(dirEntry_t));
		write_blocks(blockNumber, 1, buffer);

		printf("\n**********************************************************\n");    
		printf("Opening file %s, file newly created", name);
		printf("\n**********************************************************\n"); 

	}

	return fileID;
} // End sfs_fopen()


// Closes the given file, returns 0 if no error *************************************************
int sfs_fclose(int fileID){
	
	// See if file already closed
	if (fdTable[fileID].accessMode == 0){
		printf("sfs_fclose error 1: file already closed\n\n");
		return -1;
	}

	printf("\n**********************************************************\n");     
	printf("File %s, was just closed", dirEntryTable[fileID].fileName);
	printf("\n**********************************************************\n"); 

	// Set file to closed
	fdTable[fileID].accessMode = 0;

	// Write TODO
	free(fdTable[fileID].iNodePtr);
	
	return 0;
} // End sfs_fclose

// Writer buffer characters into disk, returns length *******************************************
int sfs_fwrite(int fileID, const char *buf, int length){

	iNode_t* iNodePtr;

	int blockNumber = 0;
	int lastBlockNumber = 0;
	int blockAddress = 0;
	int blockAddresses[12+NUMBER_INDIRECT_PTRS];
	int indirectPtrNumber = 0;
	int indirectPtrs[NUMBER_INDIRECT_PTRS];
	int fileLoc = 0;
	int bytesOccupied = 0;
	int lastByteOccupied = 0;
	int bytesLeft = 0;
	int ptrIncrement = 0;
	int i = 0;

	char buffer[BLOCK_SIZE];

	memset(buffer, 0, BLOCK_SIZE);
	memset(blockAddresses, 0, sizeof(blockAddresses));
	memset(indirectPtrs, 0, sizeof(indirectPtrs));

	// Files can only be written to if open and if space available
	if (fdTable[fileID].accessMode != 1){
		printf("sfs_fread error 1: file is not open\n\n");
		return -1;
	}
	else if (fdTable[fileID].fileLoc + length > MAX_FSIZE){
		printf("sfs_fread error 2: file size will be too large\n\n");
		return -1;
	}

	// Intialize values for method
	fileLoc = fdTable[fileID].fileLoc;
	iNodePtr = fdTable[fileID].iNodePtr;
	blockNumber = fileLoc / BLOCK_SIZE;
	bytesOccupied = fileLoc % BLOCK_SIZE;
	bytesLeft = BLOCK_SIZE - bytesOccupied;

	// Find last block that will be accessed so a loop can be run
	lastBlockNumber = (fileLoc + length) / BLOCK_SIZE;
	lastByteOccupied = (fileLoc + length) % BLOCK_SIZE;

	// Cache all block addresses in array for use
	for(int j=blockNumber; j <= lastBlockNumber; j++){
		// If blockNumber is a value that can be accessed by the direct pointer array
		if (j < 12){
			blockAddress = iNodePtr->directPtrs[j];
		
			// If no block being pointed to, find new block
			if (blockAddress == 0){
				blockAddress = findFreeBlock();
				if (blockAddress == -1){
					printf("sfs_fwrite error 3: could not find address at index %d\n\n", i);
					return -1;
				}

				iNodePtr->directPtrs[j] = blockAddress;
			}
		}

		// If block number >= 12, it is asking for indirect pointers	
		else if (j >= 12){
			// Block number of 12 would be first indirect pointer, 13 would be second, etc..
			indirectPtrNumber = j - 12;

			// If first indirect pointer number and not pointing to any blocks yet
			if(iNodePtr->indirectPtrCnt == 0){

				// Make sure a free datablock is available
				indirectPtrNumber = findFreeBlock();
				if (indirectPtrNumber == -1){
					printf("sfs_fwrite error 4: no free block for indirectPtrs\n\n");
					return -1;
				}

				// Write indirect pointers to disk
				write_blocks(iNodePtr->indirectPtr, 1, (void*) indirectPtrs);	
				indirectPtrNumber = j - 12;

			}

			// Read indirect pointer block to memory
			read_blocks(iNodePtr->indirectPtr, 1, (void*) indirectPtrs);

			// If no block address associated with this pointer
			blockAddress = indirectPtrs[indirectPtrNumber];
			if (blockAddress == 0) {
				blockAddress = findFreeBlock();
				if (blockAddress == -1){
					printf("sfs_fwrite error 5: no free block for indirectPtr\n\n");
					return -1;
				}

				iNodePtr->indirectPtrCnt++;

				// Save block address to disk
				indirectPtrs[indirectPtrNumber] = blockAddress;
				write_blocks(iNodePtr->indirectPtr, 1, (void*) indirectPtrs);

			}
		}

		// If blockNumber is invalid
		else {
			printf("sfs_fwrite error 6: invalid block address (%d) at index %d\n\n", j, i);
			return -1;
		}

		printf("**** Write: j = %d, Adrs = %d, blockNumber = %d, lastBlock = %d ****\n", j, blockAddress, 			blockNumber, lastBlockNumber);

		// If everything kosher, write in blockAddress
		blockAddresses[i] = blockAddress;
		i++;

	}

	// Restore index to 0
	i = 0;

	// If possible to write only the currently initiated block
	if (bytesOccupied + length < BLOCK_SIZE){
		// Read in block, write in new value, and write to disk
		read_blocks(blockAddresses[0], 1, buffer);
		memcpy(&buffer[bytesOccupied], buf, length);
		write_blocks(blockAddresses[0], 1, buffer);

		// Increment file values properly
		fileLoc = fileLoc + length;
		fdTable[fileID].fileLoc = fileLoc;
		//// In case pointer not at the end of the file
		if (fileLoc > iNodePtr->size){
			iNodePtr->size = fileLoc;
		}

	}
	
	// If multiple blocks must be written to
	else {
		// Write the first block
		//// Read in block, write in new value, and write to disk
		ptrIncrement = bytesLeft;
		read_blocks(blockAddresses[i], 1, buffer);
		memcpy(&buffer[bytesOccupied-1], buf, ptrIncrement);
		write_blocks(blockAddresses[i], 1, buffer);

		//// Increment file values properly
		fileLoc = fileLoc + ptrIncrement;
		fdTable[fileID].fileLoc = fileLoc;
		//// In case pointer not at the end of the file
		if (fileLoc > iNodePtr->size){
			iNodePtr->size = fileLoc;
		}
		i++;
		blockNumber++;
		buf = buf + ptrIncrement;

		// While loop will be changing whole blocks at a time
		ptrIncrement = BLOCK_SIZE;

		while (blockNumber < lastBlockNumber){
	
			// Write the next block
			//// Read in block, write in new value, and write to disk
			read_blocks(blockAddresses[i], 1, buffer);
			memcpy(buffer, buf, ptrIncrement);
			write_blocks(blockAddresses[i], 1, buffer);
	
			//// Increment file values properly
			fileLoc = fileLoc + ptrIncrement;
			fdTable[fileID].fileLoc = fileLoc;
			//// In case pointer not at the end of the file
			if (fileLoc > iNodePtr->size){
				iNodePtr->size = fileLoc;
			}
			i++;
			blockNumber++;
			buf = buf + ptrIncrement;

		}

		// Write the last block
		//// Read in block, write in new value, and write to disk
		ptrIncrement = lastByteOccupied;
		read_blocks(blockAddresses[i], 1, buffer);
		memcpy(buffer, buf, ptrIncrement); 
		write_blocks(blockAddresses[i], 1, buffer);

		//// Increment file values properly
		fileLoc = fileLoc + ptrIncrement;
		fdTable[fileID].fileLoc = fileLoc;
		//// In case pointer not at the end of the file
		if (fileLoc > iNodePtr->size){
			iNodePtr->size = fileLoc;
		}

	}

	return length;
} // End sfs_fwrite()


// Read characters from disk into buffer, returns length ****************************************
int sfs_fread(int fileID, char *buf, int length){ 

	iNode_t* iNodePtr;

	int blockNumber = 0;
	int lastBlockNumber = 0;
	int blockAddress = 0;
	int blockAddresses[12+NUMBER_INDIRECT_PTRS];
	int indirectPtrNumber = 0;
	int indirectPtrs[NUMBER_INDIRECT_PTRS];
	int fileLoc = 0;
	int bytesOccupied = 0;
	int lastByteOccupied = 0;
	int bytesLeft = 0;
	int ptrIncrement = 0;
	int i = 0;

	char buffer[BLOCK_SIZE];

	memset(buffer, 0, BLOCK_SIZE);
	memset(blockAddresses, 0, sizeof(blockAddresses));
	memset(indirectPtrs, 0, sizeof(indirectPtrs));

	// Files can only be read from if open
	if (fdTable[fileID].accessMode != 1){
		printf("sfs_fread error 1: file is not open\n\n");
		return -1;
	}

	// Intialize values for method
	fileLoc = fdTable[fileID].fileLoc;
	iNodePtr = fdTable[fileID].iNodePtr;
	blockNumber = fileLoc / BLOCK_SIZE;
	bytesOccupied = fileLoc % BLOCK_SIZE;
	bytesLeft = BLOCK_SIZE - bytesOccupied;

	// Find last block that will be accessed so a loop can be run
	lastBlockNumber = (fileLoc + length) / BLOCK_SIZE;
	lastByteOccupied = (fileLoc + length) % BLOCK_SIZE;

	// Cache all block addresses in array for use
	for(int j=blockNumber; j <= lastBlockNumber; j++){

		// If blockNumber is a value that can be accessed by the direct pointer array
		if (j < 12){
			blockAddress = iNodePtr->directPtrs[j];
		
			// If no block being pointed to, find new block
			if (blockAddress == 0){
				blockAddress = findFreeBlock();
				if (blockAddress == -1){
					printf("sfs_fread error 2: could not find address at index %d\n\n", i);
					return -1;
				}

				iNodePtr->directPtrs[j] = blockAddress;
			}
		}

		// If block number >= 12, it is asking for indirect pointers	
		else if (j >= 12){
			// Block number of 12 would be first indirect pointer, 13 would be second, etc..
			indirectPtrNumber = j - 12;

			// If first indirect pointer number and not pointing to any blocks yet
			if(iNodePtr->indirectPtrCnt == 0){

				// Make sure a free datablock is available
				indirectPtrNumber = findFreeBlock();
				if (indirectPtrNumber == -1){
					printf("sfs_fread error 3: no free block for indirectPtrs\n\n");
					return -1;
				}

				// Write indirect pointers to disk
				write_blocks(iNodePtr->indirectPtr, 1, (void*) indirectPtrs);	
				indirectPtrNumber = j - 12;

			}

			// Read indirect pointer block to memory
			read_blocks(iNodePtr->indirectPtr, 1, (void*) indirectPtrs);

			// If no block address associated with this pointer
			blockAddress = indirectPtrs[indirectPtrNumber];
			if (blockAddress == 0) {
				blockAddress = findFreeBlock();
				if (blockAddress == -1){
					printf("sfs_fwrite error 5: no free block for indirectPtr\n\n");
					return -1;
				}

				iNodePtr->indirectPtrCnt++;

				// Save block address to disk
				indirectPtrs[indirectPtrNumber] = blockAddress;
				write_blocks(iNodePtr->indirectPtr, 1, (void*) indirectPtrs);

			}
		}

		// If blockNumber is invalid
		else {
			printf("sfs_fread error 5: invalid block address at index %d\n\n", i);
			return -1;
		}

		printf("**** Read: j = %d, Adrs = %d, blockNumber = %d, lastBlock = %d ****\n", j, blockAddress, 			blockNumber, lastBlockNumber);

		// If everything kosher, write in blockAddress
		blockAddresses[i] = blockAddress;
		i++;

	}

	// Restore index to 0
	i = 0;

	

	// If only reading the currently initiated block
	if (bytesOccupied + length < BLOCK_SIZE){
		// Read in block, write in new value, and write to disk
		read_blocks(blockAddresses[i], 1, buffer);
		memcpy(buf, &buffer[bytesOccupied], length);

		// Increment file values properly
		fileLoc = fileLoc + length;
		fdTable[fileID].fileLoc = fileLoc;

	}

	// If reading multiple blocks
	else {

		// Read the first block
		ptrIncrement = bytesLeft;
		read_blocks(blockAddresses[i], 1, buffer);
		memcpy(buf, &buffer[bytesOccupied], ptrIncrement);

		//// Increment file values properly
		fileLoc = fileLoc + ptrIncrement;
		fdTable[fileID].fileLoc = fileLoc;
		i++;
		blockNumber++;
		buf = buf + ptrIncrement;

		// While loop will be changing whole blocks at a time
		ptrIncrement = BLOCK_SIZE;

		while (blockNumber < lastBlockNumber){
			// Read the next block
			read_blocks(blockAddresses[i], 1, buffer);
			memcpy(buf, buffer, ptrIncrement);

			//// Increment file values properly
			fileLoc = fileLoc + ptrIncrement;
			fdTable[fileID].fileLoc = fileLoc;
			i++;
			blockNumber++;
			buf = buf + ptrIncrement;

		}

		// Read the last block
		ptrIncrement = lastByteOccupied;
		read_blocks(blockAddresses[i], 1, buffer);
		memcpy(buf, buffer, ptrIncrement); 

		//// Increment file values properly
		fileLoc = fileLoc + ptrIncrement;
		fdTable[fileID].fileLoc = fileLoc;

	}

	return length;
} // End sfs_fread()


// Seek to the location from the beginning ******************************************************
int sfs_fseek(int fileID, int loc){

	// Files can only be worked with if open, also can't seek to NULL portion of file
	if (fdTable[fileID].accessMode != 1){
		printf("sfs_fseek error 1: file is not open\n\n");
		return -1;
	}
	else if (loc > fdTable[fileID].iNodePtr->size){
		loc = fdTable[fileID].iNodePtr->size;
	}

	// Seek to proper location
	fdTable[fileID].fileLoc = loc;

	return 0;
} // End sfs_fseek()


// Removes a file from the file system **********************************************************
int sfs_remove(char *file){

	iNode_t* iNodePtr;

	int iNodeNumber;
	int fileID;
	int blockNumber;
	int blockSpot;
	int indirectPtrs[NUMBER_INDIRECT_PTRS];

	char buffer[BLOCK_SIZE];
	char clearBlock[BLOCK_SIZE];

	memset(buffer, 0, BLOCK_SIZE);
	memset(clearBlock, 0, BLOCK_SIZE);
	iNodePtr = calloc(1, sizeof(iNode_t));

	// Find relavant iNode for search
	iNodeNumber = findINodeNumForGivenPath(file);
	if (iNodeNumber == -1){
		printf("sfs_fopen error 1: no iNode found for specified path\n\n");
		return -1;
	}

	// Search for file
	for (int i=0; i < MAX_FILES; i++){
		if (fdTable[i].iNodeNumber == iNodeNumber){
			fileID = i;
		}
	}

	printf("\n**********************************************************\n");     
	printf("File %s, was just removed from the sfs", dirEntryTable[fileID].fileName);
	printf("\n**********************************************************\n"); 

	// Clear indirect iNode block if it exists
	if (fdTable[fileID].iNodePtr->indirectPtr != 0){

		// Read indirect pointer block to memory
		read_blocks(fdTable[fileID].iNodePtr->indirectPtr, 1, (void*) indirectPtrs);

		// Free all blocks associated with indirect pointers
		for (int i=0; i < fdTable[fileID].iNodePtr->indirectPtrCnt; i++){
			
			// Clear block indirect ptr is pointing to
			write_blocks(indirectPtrs[i], 1, clearBlock);

			// Free up data block in bit map
			read_blocks(DBBMAP_ADDRESS, 1, buffer);
			buffer[(int)(indirectPtrs[i] - DATA_BLOCKS_ADDRESS)] = 0; // Corrects index for bitmap
			write_blocks(DBBMAP_ADDRESS, 1, buffer);
		}

		// Remove indirect pointers block as a whole
		//// Clear pointer block from memory
		write_blocks(fdTable[fileID].iNodePtr->indirectPtr, 1, clearBlock);

		//// Free up data block in bit map
		read_blocks(DBBMAP_ADDRESS, 1, buffer);
		buffer[(int)((fdTable[fileID].iNodePtr->indirectPtr) - DATA_BLOCKS_ADDRESS)] = 0; // Corrects index for bitmap
		write_blocks(DBBMAP_ADDRESS, 1, buffer);

	}

	// Write a cleared iNode to memory
	iNodePtr->mode = 0;   
	iNodePtr->linkCnt = 0; 
	iNodePtr->uid = 0;    
	iNodePtr->gid = 0;    
	iNodePtr->size = 0;
	memcpy(iNodePtr->directPtrs, (int[12]) {0,0,0,0,0,0,0,0,0,0,0,0}, sizeof(int[12]));
	iNodePtr->indirectPtr = 0;
	iNodePtr->indirectPtrCnt = 0;

	rwINodeTable(iNodeNumber, iNodePtr, true);

	// Clear directory table
	memset(dirEntryTable[fileID].fileName, 0, MAX_FNAME_LENGTH);
	dirEntryTable[fileID].iNodeNumber = 0;

	// Write directory entry to disk
	blockNumber = (fileID/DIRENTRIES_PER_BLOCK) + DATA_BLOCKS_ADDRESS;
	blockSpot = fileID % BLOCK_SIZE;
		
	read_blocks(blockNumber, 1, buffer);
	memcpy(&buffer[blockSpot], &dirEntryTable[fileID], sizeof(dirEntry_t));
	write_blocks(blockNumber, BLOCK_SIZE, buffer);

	// Clear values of file descriptor
	free(iNodePtr);
	free(fdTable[fileID].iNodePtr);
	fdTable[fileID].iNodeNumber = 0;
	fdTable[fileID].accessMode = 0;
	fdTable[fileID].fileLoc = 0;
	fdTable[fileID].iNodePtr = (iNode_t*) 0;

	return 0;
} // End sfs_remove()


