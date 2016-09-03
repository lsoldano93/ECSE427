// --------------------------------------------------------------------- Library inclusions ***

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h> 
#include <signal.h>
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
    printJob jobs[bufferSize];
    sem_t shmMutex;
    sem_t fullSlots;
    sem_t emptySlots;
} sharedMemory;

// --------------------------------------------------------------------- SignalHndlr        ***

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

// --------------------------------------------------------------------- main method        ***

int main(void)
{
    const char *name = "/printserv_shm";

    sem_t shmMutex;
    sem_t fullSlots;
    sem_t emptySlots;

    int fileDescriptor = 0;
    int client = 0;
    int pages = 0;
    int bufferIndex = 0;
    int numFullSlots = 0;

    sharedMemory *shmPtr;

    bool running = true;

    // Allows for Ctrl-C to quit program
    signal(SIGINT, intHandler);

    // Setup shared memory
    //// Open new memory segment
    fileDescriptor = shm_open(name, O_CREAT | O_RDWR, 0666); 
    if (fileDescriptor == -1){
        printf("\nShared memory creation failed\n");
        exit(1);
    }

    //// Set size of shared memory to alloted size
    ftruncate(fileDescriptor, sizeof(sharedMemory)); 

    // Attach shared memory
    shmPtr = (sharedMemory *) mmap(
        0,
        sizeof(sharedMemory), 
        PROT_READ | PROT_WRITE, 
        MAP_SHARED, 
        fileDescriptor, 
        0
    );
    if (shmPtr == MAP_FAILED) {
                printf("\nShared memory mapping failed\n");
                exit(1);
        }

    //// Initialize shm values to NULL 
    for (int i=0; i < bufferSize; i++){
        shmPtr->jobs[i].client = NULL;
        shmPtr->jobs[i].pages = NULL;
    }

    // Initialize the semaphores  
    shmPtr->shmMutex =  shmMutex;
    shmPtr->fullSlots = fullSlots;
    shmPtr->emptySlots = emptySlots;

    sem_init(&(shmPtr->shmMutex), 1, 1);
    sem_init(&(shmPtr->fullSlots), 1, 0);
    sem_init(&(shmPtr->emptySlots), 1, bufferSize);

    // Opening statement
    printf("\nWelcome! Print server buffer can hold %d jobs\n", bufferSize);
    printf("Max sleep time is 15 seconds\n");
    printf("Press Ctrl-C to exit at any time\n\n");

    int value;

    // Run printer server continuously
    while (keepRunning){

        // Take a job
        //// Print that sleep will occur if no jobs available
        sem_getvalue(&(shmPtr->fullSlots), &numFullSlots);
        if (numFullSlots == 0){
        printf("No jobs available, server sleeps\n");

        }

        //// Wait for a job to enter the queue
        sem_wait(&(shmPtr->fullSlots));

        //// Wait for control of memory
        sem_wait(&(shmPtr->shmMutex));

        // Determine point of access, assume buffer is circular
        while ((shmPtr->jobs[bufferIndex].client) == NULL || (shmPtr->jobs[bufferIndex].pages) == NULL){
            bufferIndex++;

            if (bufferIndex >= bufferSize){
                bufferIndex = 0;
            }

        }

        //// Save values and clear job at present buffer spot
        client = shmPtr->jobs[bufferIndex].client;
        pages = shmPtr->jobs[bufferIndex].pages;
        shmPtr->jobs[bufferIndex].client = NULL;
        shmPtr->jobs[bufferIndex].pages = NULL;

        //// Release control over shared memory and increment empty slot semaphore
        sem_post(&(shmPtr->shmMutex));
        sem_post(&(shmPtr->emptySlots));

        // Print a message 
        printf("Printing %d page(s) from client %d, found in buffer[%d]...\n", 
            pages, client, bufferIndex);

        if (pages > 15){
            pages = 15;
        }

        // Go sleep
        sleep(pages);

        // On next rotation, look in next spot of buffer
        bufferIndex++;
        if (bufferIndex >= bufferSize){
                bufferIndex = 0;
            }

    }

    //// Exit code
    sem_destroy(&(shmPtr->shmMutex));
    sem_destroy(&(shmPtr->fullSlots));
    sem_destroy(&(shmPtr->emptySlots));
    shm_unlink(name);

}

// --------------------------------------------------------------------- End                ***
