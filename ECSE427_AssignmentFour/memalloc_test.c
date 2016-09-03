// Library inclusions
#include <stdio.h>
#include <string.h>
#include "memalloc.h"

#define BLOCK_SIZE 500
#define NUMBER_POINTERS 10

int main(){

    int i = 0; 
    int error = 0;

    void* data;
    void* ptr[NUMBER_POINTERS];

    my_mallinfo();

    printf("Allocating: %d bytes\n", BLOCK_SIZE);
    data = (unsigned char*) my_malloc(BLOCK_SIZE);

    if (data == NULL) printf("test error 1: data == NULL");

    my_mallinfo();
    
    my_free((void*) data);
    
    printf("After Freeing data\n");
    my_mallinfo();
    
    for (i = 0; i < NUMBER_POINTERS; i++) {
        printf("Allocating: %d bytes\n", (NUMBER_POINTERS-i)*BLOCK_SIZE);
        ptr[i] = my_malloc((NUMBER_POINTERS-i)*BLOCK_SIZE);
        my_mallinfo();

        if (ptr == NULL){
            printf("test error %i: data == NULL", i+2);
        }
    }
    
    printf("After Allocating Pointers\n");
    my_mallinfo();

    my_free((void*) ptr[0]);
    my_free((void*) ptr[1]);
   
    printf("After Freeing First two Pointers (Merge Test)\n");
    my_mallinfo();
    
    my_free((void*) ptr[3]);
    my_free((void*) ptr[9]);
    
    printf("After Freeing two Pointers\n");
    my_mallinfo();
    
    printf("Allocating: %d bytes\n", BLOCK_SIZE);
    ptr[0] = (char*) my_malloc(BLOCK_SIZE);
    
    printf("After mallocing (First Fit test)\n");
    my_mallinfo();
  
    my_free((void*) ptr[7]);
    printf("After freeing\n");
    my_mallinfo();
   
    my_mallopt(1);
    
    printf("Allocating: %d bytes\n", BLOCK_SIZE);
    ptr[1] = (char*) my_malloc(BLOCK_SIZE);
    printf("After mallocing again(Best Fit Test)\n");
    my_mallinfo();
    
    printf("Allocating: %d bytes\n", 136);
    ptr[3] = my_malloc(136);
    printf("After mallocing big block\n");
    my_mallinfo();
    
    my_free((void*) ptr[3]);
    printf("After freeing last block (Decrease Heap Test)\n");
    my_mallinfo();
    
    return 0;
}
