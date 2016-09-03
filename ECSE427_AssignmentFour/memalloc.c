// ECSE 427 Assignment #4 - Memory Allocator
// Luke Soldano, 260447714 (C) 2015

// ---------------------------------- Parameters -----------------------------------------

// Definitions
#define BONUS_HEAP_EXTENSION_SPACE 16000 // Extend heap size by a bonus 16 kBytes

// Library inclusions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Define boolean
typedef int bool;
#define true 1
#define false 0

typedef struct block_t{
	int usage;				// Is block free or in use
	size_t size;			// Size of block
	void *previousBlock;	// Ptr to last block
	void *nextBlock;		// Ptr to next block
	char data[1];			// Data space of block

} block_t;

// Public variables
void* headerBlock = NULL;			// Block that heads the linked list
void* programBreak = 0;				// Gives ending address of heap
void* currentBreak = 0;

int mallocPolicy = 0;				// Indicates the policy to be used for malloc
int totalNumBytesAllocated = 0; 	// Gives statistics for the my_mallinfo method
int totalFreeSpace = 0; 		
int largestContFreeSpace = 0;
int totalHeapExtensions = 0;
int totalNumBlocks = 0;

// ------------------------------- Helper Functions --------------------------------------

// Extends the heap to make more room for new data
int extendHeap(int size){

	block_t* block;

	int bytes;

	char *str[80];

	sprintf(str, "Extending the heap...");
	puts(str);

	// Find current program break of data segment
	bytes = size + BONUS_HEAP_EXTENSION_SPACE;
	programBreak = sbrk(0);

	// Extend heap 
	if (sbrk(bytes) == (void*)-1){
		return 0;
	}

	// Initialize the block
	block = (block_t*) programBreak;
	block->size = (size_t) bytes;
	block->nextBlock = NULL;
	block->usage = 0;

	// If first extension, previous block does not exist
	if (totalHeapExtensions == 0){
		block->previousBlock = NULL;
		headerBlock = programBreak;
	}
	else {
		block->previousBlock = currentBreak;	
		((block_t*) currentBreak)->nextBlock = programBreak;
	}

	// Update stats
	currentBreak = programBreak;
	totalFreeSpace += bytes;  
	totalHeapExtensions++;
	totalNumBlocks++;

	return 1;

} // End extendHeap

// Function for splitting block into free and used block
void splitBlock(int size){

	block_t* newBlock;

	char *str[80];

	newBlock = currentBreak + size;

	sprintf(str, "Splitting a block...");
	puts(str);

	// Intialize new block
	newBlock->usage = 0;
	newBlock->size = ((block_t*) currentBreak)->size - (size_t) size; 
	newBlock->previousBlock = currentBreak;
	newBlock->nextBlock = ((block_t*) currentBreak)->nextBlock;

	// Edit old block
	((block_t*) currentBreak)->nextBlock = (void*) newBlock;

	totalNumBytesAllocated += size;
	totalFreeSpace -= size;
	totalNumBlocks++;

	return;

} // End splitBlock

/* Function for merging free blocks into larger free block 
   Returns starting address of new merged block */
void mergeBlockWithNext(){

	block_t* newBlock;

	char *str[80];

	sprintf(str, "Merging two blocks...");
	puts(str);

	newBlock = (block_t*) currentBreak;

	newBlock->size = ((block_t*) currentBreak)->size + ((block_t*)((block_t*) currentBreak)->nextBlock)->size;
	newBlock->previousBlock = ((block_t*) currentBreak)->previousBlock;
	newBlock->nextBlock = ((block_t*) ((block_t*) currentBreak)->nextBlock)->nextBlock;
	newBlock->usage = 0;

	if (newBlock->size > largestContFreeSpace){
		largestContFreeSpace = newBlock->size;
	}

	totalNumBlocks--;

	return;

} // End splitBlock

// Function to find available free block based off of chosen malloc policy
int findBlock(int bytesToAllocate){

	block_t* block;
	block_t* bestBlock;

	bool bestBlockFound = false;

	char *str[80];

	int i = 0;

	sprintf(str, "Finding a block...");
	puts(str);

	// Perform first fit memory allocation process
	if (mallocPolicy == 0 && totalHeapExtensions != 0){

		// Initial block in search is lastBlock provided 
		block = (block_t*) headerBlock;
		currentBreak = headerBlock;

		// Search the linked list of blocks for one that fits the requirements
		while (i < totalNumBlocks){
			
			if (i != 0){
				currentBreak += block->size;
				if (block->usage == 1){
					currentBreak += sizeof(block_t);
				}

				block = (block_t*) block->nextBlock;
			}

			sprintf(str, "Iter: %d, Block: %d, Size: %d, Usage: %d", i, (int*) block, ((int) block->size), block->usage);
			puts(str);

			// Return block if free and spacious enough
			if (block->usage == 0 && (((int) block->size) >= bytesToAllocate)){
				return 1;
			}
				
			i++;
		}
	}

	// Perform best fit memory allocation process
	else if (mallocPolicy == 1 && totalHeapExtensions != 0){

		// Initial block in search is lastBlock provided 
		block = headerBlock;
		currentBreak = headerBlock;

		// Search the linked list of blocks for one that fits the requirements
		while (i < totalNumBlocks){
			
			if (i != 0){
				currentBreak += block->size;
				if (block->usage == 1){
					currentBreak += sizeof(block_t);
				}

				block = (block_t*) block->nextBlock;
			}

			sprintf(str, "Iter: %d, Block: %d, Size: %d, Usage: %d", i, (int*) block, ((int) block->size), block->usage);
			puts(str);

			// If block free and spacious enough analyze for best fit
			if (block->usage == 0 && (((int) block->size) >= bytesToAllocate)){

				// For first best block
				if (!bestBlockFound){
					bestBlockFound = true;
					bestBlock = block;
				}
				
				// For comparing future blocks found
				else if ((int) block->size - bytesToAllocate < (int) bestBlock->size - bytesToAllocate){
					bestBlock = block;
				}
			}

			i++;

			// Return best block if found and search over
			if (bestBlockFound && i == totalNumBlocks){
				currentBreak = (void*) bestBlock;
				return 1;
			}
		}
	}

	// First block allocated or
	// If no blocks match necessary criteria, extend the heap
	return extendHeap(bytesToAllocate);

} // End findBlock

// ----------------------------------- Methods -------------------------------------------
/* 
This function returns void pointer that we can assign to any C pointer. If memory could 
not be allocated, then my_malloc() returns NULL and sets a global error string variable 
given by the following declaration.
*/
void* my_malloc(int size){

	int bytesToAllocate; 
	int error;

	char *str[80];

	// Determine actual size needed 
	bytesToAllocate = sizeof(block_t) + size;

	error = findBlock(bytesToAllocate); 
	if (error == 0) {
		sprintf(str, "** my_malloc error 1: Ran out of memory! \n");
		puts(str);
		return NULL;
	}

	// See if possible to split block 
	if (((block_t*)currentBreak)->size > bytesToAllocate){
		splitBlock(bytesToAllocate);
	}
	else{
		totalNumBytesAllocated += bytesToAllocate;
		totalFreeSpace -= bytesToAllocate;
	}

	// Set values of block
	((block_t*) currentBreak)->usage = 1;
	((block_t*) currentBreak)->size = (size_t) size;

	// Offset returned address by metadata space
	return currentBreak + sizeof(block_t); 

} // End my_malloc

/*
This function deallocates the block of memory pointed by the ptr argument. 
The ptr should be an address previously allocated by the Memory Allocation Package.
The my_free() should reduce the program break if the top free block is larger than 128 Kbytes.
*/
void my_free(void *ptr){

	block_t* block;

	int size;

	char *str[80];

	// If the argument of my_free() is NULL, then the call should not free any thing.
	if (ptr == NULL){
		sprintf(str, "** my_free error 1: Null pointer input\n");
		puts(str);
		return;
	}

	// Deal with metadata offset within pointer
	currentBreak = ptr - sizeof(block_t);
	block = (block_t*) currentBreak;
	size = (int) block->size + sizeof(block_t);

	// Free block
	block->usage = 0;
	block->size = (size_t) size;

	// Merge block with previous if also free
	if (block->previousBlock != NULL){
		if (((block_t*) block->previousBlock)->usage == 0){
			currentBreak -= ((block_t*) block->previousBlock)->size;
			mergeBlockWithNext();
			block = (block_t*) currentBreak;
		}
	}

	// Merge block with next if also free
	if (block->nextBlock != NULL){
		if (((block_t*) block->nextBlock)->usage == 0){
			mergeBlockWithNext();
		}
	}

	// Make sure header isn't lost
	if (block->previousBlock == NULL){
		headerBlock = currentBreak;
	}

	// Update stats
	totalNumBytesAllocated -= size;
	totalFreeSpace += size;

	return;

} // End my_free

/*
This function specifies the memory allocation policy. You need implement two policies as 
part of this assignment: first fit and best fit. Refer to the lecture slides for more 
information about these policies.
*/
void my_mallopt(int policy){

	char *str[80];

	// A policy value of 0 indicates first fit
	if (policy == 0){
		sprintf(str, "First fit policy chosen!\n");
		puts(str);
		mallocPolicy = 0;
	}
	else if (policy == 1){
		sprintf(str, "Best fit policy chosen!\n");
		puts(str);
		mallocPolicy = 1;
	}
	else {
		sprintf(str, "** my_mallopt error 1: Invalid malloc policy input\n");
		puts(str);
		exit(0);
	}

	return;

} // End my_mallopt

/*
This function prints statistics about the memory allocation performed so far by the 
library. You need to include the following parameters: total number of bytes allocated, 
total free space, largest contiguous free space, and others of your choice.
*/
void my_mallinfo(){

	block_t* block;

	int largestFreeSpaceFound = 0;
	int i = 0;

	char *str[80];

	// Find largest continuous free space
	if (totalHeapExtensions > 0){
		block = headerBlock;

		while (i < totalNumBlocks){

			//sprintf(str, "Iter: %d, Blocks: %d", i, totalNumBlocks);
			//puts(str);

			if (i != 0){
				block = (block_t*) block->nextBlock;
			}

			if (block->usage == 0 && (int) block->size > largestFreeSpaceFound){
				largestFreeSpaceFound = (int) block->size;
			}

			i++;
		}
	}

	largestContFreeSpace = largestFreeSpaceFound;

	// Print memory allocation statistics
	sprintf(str, "\n********************************************************\n");
	puts(str);

	// Determine policy
	if (mallocPolicy == 0){
		sprintf(str, "   Memory allocation policy: first fit");
	}
	else {
		sprintf(str, "   Memory allocation policy: best fit");
	}
	puts(str);

	sprintf(str, "   Total number of bytes allocated: %d", totalNumBytesAllocated);
	puts(str);
	sprintf(str, "   Total free space in bytes: %d", totalFreeSpace);
	puts(str);
	sprintf(str, "   Largest contiguous free space in bytes: %d", largestContFreeSpace);
	puts(str);
	sprintf(str, "   Total number of heap extensions: %d\n", totalHeapExtensions);
	puts(str);
	sprintf(str, "********************************************************\n");
	puts(str);

	return;

} // End my_mallinfo

/* 
The possibility of error in memory allocation may be small, but it is important to 
provide mechanisms to handle the error conditions.
*/
extern char *my_malloc_error(){


	return;
} // End my_malloc_error 




