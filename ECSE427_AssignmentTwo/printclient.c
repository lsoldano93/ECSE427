// --------------------------------------------------------------------- Library inclusions ***

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h> 
#include <sys/shm.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/mman.h> 
#include <fcntl.h>

#define bufferSize 3

// --------------------------------------------------------------------- Constructs         ***

typedef int bool;
#define true 1
#define false 0

typedef struct
{
    int client;
    int pages;
} printJob;

typedef struct
{
    printJob jobs[3];
    sem_t shmMutex;
    sem_t fullSlots;
    sem_t emptySlots;
} sharedMemory;

// --------------------------------------------------------------------- main method        ***

int main(int argc, char **argv)
{
	const char *name = "/printserv_shm";

    printJob jobRequest;

    sharedMemory *shmPtr;

    int argCount = 0;
    int fileDescriptor = 0;
    int bufferIndex = 0;
    int numEmptySlots = 3;

    // Gather arguments from user
    if(argc != 3){
    	printf("\nNumber of arguments entered is incorrect\n");
    	printf("User should enter arguments in format '/progname client# #pages'\n\n");
    	exit(-1);
    }

    // Attach shared memory
    fileDescriptor = shm_open(name, O_RDWR, 0666); // TODO: Why 0666?
    if (fileDescriptor == -1){
        printf("Shared memory access failed\n");
        exit(1);
    }

    shmPtr = (sharedMemory *) mmap(
		0,
		sizeof(sharedMemory), 
		PROT_READ | PROT_WRITE, 
		MAP_SHARED, 
		fileDescriptor, 
		0
	);
	if (shmPtr == -1){
        printf("Shared memory pointer mapping failed\n");
        exit(1);
    }

    // Place parameters

    // Get job parameters

    // Create a job
    //// No error handling for arguments being non integer values
    //// I feel as though this is not required within the scope of the assignment
    jobRequest.client = atoi(argv[1]);
    jobRequest.pages = atoi(argv[2]);

    // Put a job
    //// Note that client had to wake up if no empty slots were available initially
    sem_getvalue(&(shmPtr->emptySlots), &numEmptySlots);
    if (numEmptySlots == 0){
    	printf("No buffer space available, client %d sleeps\n", jobRequest.client);

    }

    //// Wait for a free spot in the queue
    sem_wait(&(shmPtr->emptySlots)); 

    //// Wait for control of memory
    sem_wait(&(shmPtr->shmMutex));

    //// Determine point of access, assume buffer is circular
    while ((shmPtr->jobs[bufferIndex].client) != NULL && (shmPtr->jobs[bufferIndex].pages) != NULL){
        bufferIndex++;

        if (bufferIndex >= bufferSize){
            bufferIndex = 0;
        }

    }

    //// Once empty place in buffer is found, insert job request
    shmPtr->jobs[bufferIndex] = jobRequest;

    printf("Client %d has %d pages to print, request placed in buffer[%d]\n",
    	jobRequest.client, jobRequest.pages, bufferIndex); // TODO: Remove

    //// Release control over shared memory and increment full slot semaphore
    sem_post(&(shmPtr->shmMutex));
    sem_post(&(shmPtr->fullSlots));

    // Release shared memory
    shmdt(shmPtr);

}

// --------------------------------------------------------------------- End                ***