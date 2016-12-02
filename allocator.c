//
//  COMP1927 Assignment 1 - Vlad: the memory allocator
//  allocator.c ... implementation
//
//  Created by Liam O'Connor on 18/07/12.
//  Modified by John Shepherd in August 2014, August 2015
//  Copyright (c) 2012-2015 UNSW. All rights reserved.
//

#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#define FREE_HEADER_SIZE  sizeof(struct free_list_header)  
#define ALLOC_HEADER_SIZE sizeof(struct alloc_block_header)  
#define MAGIC_FREE     0xDEADBEEF
#define MAGIC_ALLOC    0xBEEFDEAD

#define BEST_FIT       1
#define WORST_FIT      2
#define RANDOM_FIT     3

#define TRUE           1
#define FALSE          0
#define DEBUGGING      0

typedef unsigned char byte;
typedef u_int32_t vsize_t;
typedef u_int32_t vlink_t;
typedef u_int32_t vaddr_t;

typedef struct free_list_header {
   u_int32_t magic;  // ought to contain MAGIC_FREE
   vsize_t size;     // # bytes in this block (including header)
   vlink_t next;     // memory[] index of next free block
   vlink_t prev;     // memory[] index of previous free block
} free_header_t;

typedef struct alloc_block_header {
   u_int32_t magic;  // ought to contain MAGIC_ALLOC
   vsize_t size;     // # bytes in this block (including header)
} alloc_header_t;

// Global data

static byte *memory = NULL;   // pointer to start of allocator memory
static vaddr_t free_list_ptr; // index in memory[] of first block in free list
static vsize_t memory_size;   // number of bytes malloc'd in memory[]
static u_int32_t strategy;    // allocation strategy (by default BEST_FIT)

// Private functions

static void vlad_merge();
int isPowerOf2(int num);
int numRegions(u_int32_t magic);


// Input: size - number of bytes to make available to the allocator
// Output: none              
// Precondition: Size >= 1024
// Postcondition: `size` bytes are now available to the allocator
// 
// (If the allocator is already initialised, this function does nothing,
//  even if it was initialised with different size)

void vlad_init(u_int32_t size)
{
   if (DEBUGGING) printf("CALLED VLAD_INIT\n");
   
   // settining initial memory size before checking it is a power of 2
   memory_size = size;
   
   // If sizeTest is not 1, then it would have a multiple that is not 2
   if (!isPowerOf2(size)){
      // Start sizeTest from 1
      memory_size = 1;
      // and double it until it is greater than size
      while (memory_size < size){
         memory_size *= 2;
      }
   }
   
   if (DEBUGGING)
      printf("memory_size = %d\n", memory_size);
   
   // dummy statements to keep compiler happy
   // remove them when you implement your solution
   memory = (byte *) malloc(memory_size);
   
   if (DEBUGGING)
      printf("memory = %p\n\n",memory);
   
   // Check if malloc was able to allocate initial memory
   if (memory == NULL){
      fprintf(stderr, "vlad_init: Insufficient memory\n");
      exit(EXIT_FAILURE);
   }
   
   free_list_ptr = (vaddr_t)0;
   strategy = BEST_FIT;
   
   // setting up free block header
   free_header_t *free_header = (free_header_t *) memory;
   free_header->magic = MAGIC_FREE;
   free_header->size = memory_size;
   free_header->next = 0;
   free_header->prev = 0;
   
   // setting the free list pointer to point to the header of the only free block
   free_list_ptr = 0;
}


// Input: n - number of bytes requested
// Output: p - a pointer, or NULL
// Precondition: n < size of largest available free block
// Postcondition: If a region of size n or greater cannot be found, p = NULL 
//                Else, p points to a location immediately after a header block
//                      for a newly-allocated region of some size >= 
//                      n + header size.

void *vlad_malloc(u_int32_t n)
{
   if (DEBUGGING) printf("CALLED VLAD_MALLOC\n");
   
   // setting the number of bytes minimum that the allocated region must contain
   u_int32_t bytes = ALLOC_HEADER_SIZE + n;
   
   if (DEBUGGING) printf("bytes before = %d\n",bytes);
   
   // test if it is a multiple of 4 and also must be greater than size of free_header_t = 16
   while ( !(bytes % 4 == 0 && bytes >= FREE_HEADER_SIZE) ) {
      // if it isn't, then add 1 to it until it is a multiple of 4
      bytes += 1;
   }
   
   // setting threshold of maximum memory block to allocate without splitting
   int threshold = bytes + 2*FREE_HEADER_SIZE;
   
   if (DEBUGGING){
      printf("bytes after = %d\n",bytes);
      printf("threshold = %d\n",threshold);
   }
   
   // declaring pointer variables for free and headers
   free_header_t *free_ptr = (free_header_t *) (memory+free_list_ptr);
   
   // checking to see if the free region is valid otherwise user might have seg faulted
   if (free_ptr->magic != MAGIC_FREE){
      fprintf(stderr, "vald_alloc: Memory corruption\n");
      exit(EXIT_FAILURE);
   }
   
   // create the void pointer to be returned to the client
   byte *rtnPtr = NULL;
   
   // save a copy of free_region to compare to original if it loops back to it
   free_header_t *free_ptr_copy = free_ptr;
   
   // defining variable to store the smallest region greater than 'bytes'
   free_header_t *best_free_ptr = free_ptr;
   int best_found = FALSE;
   
   // ============================= BESR FIT =================================
   // cycling through the list of free pointers and storing the smallest size one
   // greater than 'bytes' in best_free_ptr
   do {
      
      // check if the size of the current free region is big enough to contain 'bytes'
      if (free_ptr->size < bytes){
         // region too small, move to the next free list
         if (DEBUGGING) printf("too small, only %d bytes\n", free_ptr->size);
         
         // checking to see if the free region is valid every time
         if (free_ptr->magic != MAGIC_FREE){
            fprintf(stderr, "vald_alloc: Memory corruption\n");
            exit(EXIT_FAILURE);
         }
      // we found a big enough region, but gotta find the smallest region
      } else {
         if (DEBUGGING) printf("big enough, but is it the smallest\n");
         
         if (best_free_ptr->size < bytes || free_ptr->size < best_free_ptr->size)
            best_free_ptr = free_ptr;
         
         if (best_free_ptr->size >= bytes)
            best_found = TRUE;
            
         if (DEBUGGING) printf("best_free_ptr at index %d with size %d\n",
                              (vaddr_t) best_free_ptr - (vaddr_t) memory, best_free_ptr->size);
      }
      free_ptr = (free_header_t *) (memory+free_ptr->next);
      
   } while (free_ptr != free_ptr_copy);
   
   // if the best pointer is the same as the free_list_ptr then it is at the start
   int still_at_start = (best_free_ptr == free_ptr_copy) ? TRUE: FALSE;
   
   // case where a region of sufficient size was found
   if (best_found){
      vaddr_t free_index = (vaddr_t) best_free_ptr - (vaddr_t) memory;
      
      // making alloc_ptr point to the start of the current free region
      alloc_header_t *alloc_ptr = (alloc_header_t *) best_free_ptr;
      
      // now checking for whether the free region needs to be split
      if (best_free_ptr->size < threshold){
         // simply allocate that whole region to the request
         
         if (DEBUGGING)
            printf("  region is smaller than threshold\n");
         
         // check if the whole region is the only region because the invariant is that
         // there must be at least one free region
         if (best_free_ptr->next != free_index){
            // if it doesn't point to itself make the void pointer point to first byte after the header
            alloc_ptr->magic = MAGIC_ALLOC;
            alloc_ptr->size = best_free_ptr->size;
            rtnPtr = (byte *) alloc_ptr + ALLOC_HEADER_SIZE;
            
            // shift free_list_pointer
            if (still_at_start) free_list_ptr = best_free_ptr->next;
            
            // then need to update prev and next pointers of surrounding free regions
            // to skip the newly allocated one
            free_header_t *new_prev_ptr = (free_header_t *) (memory + best_free_ptr->prev);
            free_header_t *new_next_ptr = (free_header_t *) (memory + best_free_ptr->next);
            new_prev_ptr->next = best_free_ptr->next;
            new_next_ptr->prev = best_free_ptr->prev;
            
         } else {
            if (DEBUGGING)
               printf("  it is the only region so can't allocate whole region\n");
            
            // if it is the only region, it cannot be allocated
            rtnPtr = NULL;
         }
         
      } else {
         // need to split the region into a threshold size allocated region
         // and a (free_list->size - threshold) size free region
         // second region is 'threshold' bytes ahead of free_list_ptr
         
         // creating new pointer to point to the after-split free region
         byte *newPtr = (byte *) (memory + free_index + bytes);
         
         if (DEBUGGING){
            printf("  region is too big to allocate whole region\n");
            printf("  free region split at %p or index %d\n",newPtr,free_index+bytes);
         }
         
         // casting the newPtr to another pointer that is a free_header_t pointer
         // in order to update the struct elements
         free_header_t *new_free_ptr = (free_header_t *) newPtr;
         new_free_ptr->magic = MAGIC_FREE;
         new_free_ptr->size = best_free_ptr->size - bytes;
         
         // before setting next and prev, there are 2 cases
         if (best_free_ptr->next == free_index){
            // case 1 where the free region is the only free one and points to itself.
            // hence, the next free region still is the only one and points to itself
            new_free_ptr->next = best_free_ptr->next + bytes;
            new_free_ptr->prev = best_free_ptr->prev + bytes;
         } else {
            // case 2 where there are multiple free regions, in which case the next and
            // prev would point to a region independent of the current one
            new_free_ptr->next = best_free_ptr->next;
            new_free_ptr->prev = best_free_ptr->prev;
            
            // now updating surrounding regions to point to shifted new free region
            free_header_t *new_next_ptr = (free_header_t *) (memory + new_free_ptr->next);
            free_header_t *new_prev_ptr = (free_header_t *) (memory + new_free_ptr->prev);
            new_next_ptr->prev = free_index + bytes;
            new_prev_ptr->next = free_index + bytes;
         }
         
         if (DEBUGGING){
            printf("   new_free_ptr->size = %d\n",new_free_ptr->size);
            printf("   new_free_ptr->next = %d\n",new_free_ptr->next);
            printf("   new_free_ptr->prev = %d\n",new_free_ptr->prev);
         }
         
         // setting the header values to the correct allocated ones
         alloc_ptr->magic = MAGIC_ALLOC;
         alloc_ptr->size = bytes;
         
         rtnPtr = (byte *) alloc_ptr + ALLOC_HEADER_SIZE;
         if (still_at_start) free_list_ptr += bytes;
         
      }
      
      if (DEBUGGING){
         printf("  free_list-ptr = %d\n",free_list_ptr);
         printf("  rtnPtr = %p at index %d\n",rtnPtr, (vaddr_t) rtnPtr - (vaddr_t) memory);
         sleep(0.2);
      }
      
   }
   // ============================================================================
   

   // ============================= RANDOM FIT =================================
   /*
   do {
    
      // check if the size of the current free region is big enough to contain 'bytes'
      if (free_ptr->size < bytes){
         // region too small, move to the next free list
         if (DEBUGGING) printf("too small, only %d bytes\n", free_ptr->size);
         free_ptr = (free_header_t *) (memory+free_ptr->next);
         still_at_start = FALSE;
         
         // checking to see if the free region is valid every time
         if (free_ptr->magic != MAGIC_FREE){
            fprintf(stderr, "vald_alloc: Memory corruption\n");
            exit(EXIT_FAILURE);
         }
         
      // otherwise we have found a big enough region greater than size 'bytes'
      } else {
         vaddr_t free_index = (vaddr_t) free_ptr - (vaddr_t) memory;
         
         if (DEBUGGING)
            printf(" found big enough region size %d at index %d\n",free_ptr->size, free_index);
         
         // making alloc_ptr point to the start of the current free region
         alloc_header_t *alloc_ptr = (alloc_header_t *) free_ptr;
         
         // now checking for whether the free region needs to be split
         if (free_ptr->size < threshold){
            // simply allocate that whole region to the request
            
            if (DEBUGGING)
               printf("  region is smaller than threshold\n");
            
            // check if the whole region is the only region because the invariant is that
            // there must be at least one free region
            if (free_ptr->next != free_index){
               // if it doesn't point to itself make the void pointer point to first byte after the header
               rtnPtr = (byte *) free_ptr + ALLOC_HEADER_SIZE;
               free_list_ptr = free_ptr->next;
               
               // then need to update prev and next pointers of surrounding free regions
               // to skip the newly allocated one
               free_header_t *new_prev_ptr = (free_header_t *) (memory + free_ptr->prev);
               free_header_t *new_next_ptr = (free_header_t *) (memory + free_ptr->next);
               new_prev_ptr->next = free_ptr->next;
               new_next_ptr->prev = free_ptr->prev;
               
            } else {
               if (DEBUGGING)
                  printf("  it is the only region so can't allocate whole region\n");
               
               // if it is the only region, it cannot be allocated
               rtnPtr = NULL;
               
               break;
            }

         } else {
            // need to split the region into a threshold size allocated region
            // and a (free_list->size - threshold) size free region
            // second region is 'threshold' bytes ahead of free_list_ptr
            
            // creating new pointer to point to the after-split free region
            byte *newPtr = (byte *) (memory + free_index + bytes);
            
            if (DEBUGGING){
               printf("  region is too big to allocate whole region\n");
               printf("  free region split at %p or index %d\n",newPtr,free_index+bytes);
            }
            
            // casting the newPtr to another pointer that is a free_header_t pointer
            // in order to update the struct elements
            free_header_t *new_free_ptr = (free_header_t *) newPtr;
            new_free_ptr->magic = MAGIC_FREE;
            new_free_ptr->size = free_ptr->size - bytes;
            
            // before setting next and prev, there are 2 cases
            if (free_ptr->next == free_index){
               // case 1 where the free region is the only free one and points to itself.
               // hence, the next free region still is the only one and points to itself
               new_free_ptr->next = free_ptr->next + bytes;
               new_free_ptr->prev = free_ptr->prev + bytes;
            } else {
               // case 2 where there are multiple free regions, in which case the next and
               // prev would point to a region independent of the current one
               new_free_ptr->next = free_ptr->next;
               new_free_ptr->prev = free_ptr->prev;
               
               // now updating surrounding regions to point to shifted new free region
               free_header_t *new_next_ptr = (free_header_t *) (memory + new_free_ptr->next);
               free_header_t *new_prev_ptr = (free_header_t *) (memory + new_free_ptr->prev);
               new_next_ptr->prev = free_index + bytes;
               new_prev_ptr->next = free_index + bytes;
            }
            
            if (DEBUGGING){
               printf("   new_free_ptr->size = %d\n",new_free_ptr->size);
               printf("   new_free_ptr->next = %d\n",new_free_ptr->next);
               printf("   new_free_ptr->prev = %d\n",new_free_ptr->prev);
            }
            
            rtnPtr = (byte *) free_ptr + ALLOC_HEADER_SIZE;
            if (still_at_start) free_list_ptr += bytes;
            
         }
         
         if (DEBUGGING){
            printf("  free_list-ptr = %d\n",free_list_ptr);
            printf("  rtnPtr = %p at index %d\n",rtnPtr, (vaddr_t) rtnPtr - (vaddr_t) memory);
            sleep(0.2);
         }
         
         // setting the header values to the correct allocated ones
         alloc_ptr->magic = MAGIC_ALLOC;
         alloc_ptr->size = bytes;
         
         break;
      }
    
   } while (free_ptr != free_ptr_copy);
   */
   // ============================================================================
   
   return (void *) rtnPtr;
}


// Input: object, a pointer.
// Output: none
// Precondition: object points to a location immediately after a header block
//               within the allocator's memory.
// Postcondition: The region pointed to by object has been placed in the free
//                list, and merged with any adjacent free blocks; the memory
//                space can be re-allocated by vlad_malloc

void vlad_free(void *object)
{
   if (DEBUGGING) printf("CALLED VLAD_FREE\n");
   
   // assert case that free only works on non-null pointer
   if (object == NULL){
      fprintf(stderr,"cannot free null pointer\n");
      exit(EXIT_FAILURE);
   }
   
   // set up pointer to alloc header to pre index magic
   alloc_header_t *alloc_ptr = (alloc_header_t *) object;
   vaddr_t object_index = (vaddr_t) alloc_ptr - (vaddr_t) memory;
   
   // preindexing alloc header from object
   alloc_ptr--;
   
   if (DEBUGGING){
      printf("to free object at %p at index %d\n",object, object_index);
      printf("alloc_header at %p\n",alloc_ptr);
   }
   
   if (alloc_ptr->magic != MAGIC_ALLOC){
      fprintf(stderr, "vlad_free: Attempt to free non-allocated memory\n");
      exit(EXIT_FAILURE);
   } else {
      // re-defining the free_header of the ex-allocated memory
      free_header_t *new_free_ptr = (free_header_t *) alloc_ptr;
      vaddr_t new_free_ptr_index = (vaddr_t) new_free_ptr - (vaddr_t) memory;
      new_free_ptr->magic = MAGIC_FREE;
      
      
      // finding pointers to the free regions adjacent to pointed one
      free_header_t *free_ptr = (free_header_t *) (memory + free_list_ptr);
      
      if (DEBUGGING){
         printf(" free_list_ptr (free_ptr) is at %p or index %d\n",free_ptr,free_list_ptr);
         printf(" newly freed region (new_free_ptr) at %p or index %d\n", new_free_ptr, new_free_ptr_index);
      }
      
      if (free_ptr->next == free_list_ptr){
         // case 1: where the free_list_pointed region is the only free list
         free_ptr->next = (vaddr_t) new_free_ptr - (vaddr_t) memory;
         free_ptr->prev = free_ptr->next;
         new_free_ptr->next = free_list_ptr;
         new_free_ptr->prev = new_free_ptr->next;
         
         if (DEBUGGING){
            printf("\tfirst free region: next = %d, prev = %d\n",free_ptr->next,free_ptr->prev);
            printf("\tnew free region: next = %d, prev = %d\n",new_free_ptr->next,new_free_ptr->prev);
            printf("\tnew free region size = %d\n",new_free_ptr->size);
         }
         
      } else {
         // case 2: where there are multiple free regions, hence need to cycle through
         
         // tracking pointer for cycling through free region list
         free_header_t* curr = free_ptr;
         free_header_t* next;
         free_header_t* prev;
         
         // indicators for the special case where the new free isn't between any two adgacent
         // free regions
         int lowest = FALSE;
         int highest = FALSE;
         
         // case 2.1: where the first free region is higher in memory than newly freed region
         if (free_ptr > new_free_ptr){
            // hence, track backwards until free region (curr) is less than new free region
            do {
               if (DEBUGGING) printf("case 2.1: curr is %d\n",(vaddr_t)curr - (vaddr_t)memory);
               curr = (free_header_t *) (memory + curr->prev);
            } while ( !(curr == free_ptr || curr < new_free_ptr) );
            
            // if the current free pointer is still greater than the new free pointer, then all
            // the free regions are higher than the new one, which is then the lowest
            if (curr > new_free_ptr) lowest = TRUE;

         // case 2.2: where the first free region is lower in memory than newly freed region
         } else if (free_ptr < new_free_ptr){
            // hence, track forwards until free region is higher than new free region
            do {
               if (DEBUGGING) printf("case 2.2: curr is %d\n",(vaddr_t)curr - (vaddr_t)memory);
               curr = (free_header_t *) (memory + curr->next);
            } while ( !(curr == free_ptr || curr > new_free_ptr) );

            // if the current free pointer is still less than the new free pointer, then all
            // the free regions are lower than the new one, which is then the highest
            if (curr < new_free_ptr) highest = TRUE;
         }
         
         // if curr is pointing to the region just after new free need to get address of previous free
         //
         // if the new free region is the lowest or highest, curr will always stop at free_list_ptr
         // which is always the next of both those cases.
         if (curr > new_free_ptr || lowest || highest){
            prev = (free_header_t *) (memory + curr->prev);
            next = curr;
            
         // else if curr is pointing to the region just before new free
         // need to get address of next free
         } else if (curr < new_free_ptr){
            next = (free_header_t *) (memory + curr->next);
            prev = curr;
         }
         
         // after getting pointers to next and prev, add new free between them
         new_free_ptr->next = (vaddr_t) next - (vaddr_t) memory;
         new_free_ptr->prev = (vaddr_t) prev - (vaddr_t) memory;
         prev->next = (vaddr_t) new_free_ptr - (vaddr_t) memory;
         next->prev = (vaddr_t) new_free_ptr - (vaddr_t) memory;
         
         if (DEBUGGING){
            printf(" pointer to surroundings:");
            printf(" next = %d, prev = %d\n",new_free_ptr->next,new_free_ptr->prev);
            
            printf(" next and prev's reciprocating pointers:");
            printf(" next->prev = %d ,", ((free_header_t *)(memory+new_free_ptr->next)) -> prev);
            printf(" prev->next = %d\n", ((free_header_t *)(memory+new_free_ptr->prev)) -> next);
         }
      }
      
      
      // finding index of newly freed memory to set free_list_ptr to it if it is
      // greater than this value
      vaddr_t new_free_index = (vaddr_t) new_free_ptr - (vaddr_t) memory;
      
      if (DEBUGGING){
         printf("free_index = %d\n",new_free_index);
         printf("free_list_ptr = %d\n",free_list_ptr);
      }
      
      if (free_list_ptr > new_free_index) {
         free_list_ptr = new_free_index;
      }
      
      if (DEBUGGING)
         printf("now free_list_ptr = %d\n",free_list_ptr);
      
   }
   
   if (DEBUGGING) { printf("\n"); vlad_stats(); }
   
   vlad_merge();
}

// Input: current state of the memory[]
// Output: new state, where any adjacent blocks in the free list
//            have been combined into a single larger block; after this,
//            there should be no region in the free list whose next
//            reference is to a location just past the end of the region

static void vlad_merge()
{
   if (DEBUGGING) printf("CALLED VLAD_MERGE\n");
   
   // tracker starting from start of free list
   vaddr_t curr = free_list_ptr;
   
   // pointer to free_header at curr to dereference next
   free_header_t *curr_ptr = (free_header_t *) (memory + curr);
   
   while (curr_ptr->prev < free_list_ptr){
      free_list_ptr = curr_ptr->prev;
      curr = free_list_ptr;
      curr_ptr = (free_header_t *) (memory + curr);
   }
   
   int still_at_start = TRUE;
   do {
      
      // find out number of bytes the next free region is ahead of curr
      vaddr_t bytes_ahead = curr_ptr->next - curr;
      
      if (DEBUGGING){
         printf("curr = %d, curr_ptr->next = %d, ",curr,curr_ptr->next);
         printf("bytes ahead = %d, curr_ptr->size = %d\n",bytes_ahead,curr_ptr->size);
         
      }
      
      
      if (curr_ptr->size == bytes_ahead){
         // then the next free region is right after the current one
         free_header_t *next_ptr = (free_header_t *) (memory + curr_ptr->next);
         free_header_t *next_next_ptr = (free_header_t *) (memory + next_ptr->next);
         
         // first update current next to skip the next one
         curr_ptr->next = next_ptr->next;
         
         // then update prev of curr->next->next to skip curr->next
         next_next_ptr->prev = curr;
         
         // update size of curr to be addition of two sizes
         curr_ptr->size += next_ptr->size;
         next_ptr->magic = 0;
         
         if (curr != free_list_ptr){
            still_at_start = FALSE;
         }
      } else {
         curr = curr_ptr->next;
         curr_ptr = (free_header_t *) (memory + curr);
         still_at_start = FALSE;
      }
      
   } while (curr != free_list_ptr || still_at_start);
   
}

// Stop the allocator, so that it can be init'ed again:
// Precondition: allocator memory was once allocated by vlad_init()
// Postcondition: allocator is unusable until vlad_int() executed again

void vlad_end(void)
{
   if (memory != NULL)
      free(memory);
   memory = NULL;
}


// Precondition: allocator has been vlad_init()'d
// Postcondition: allocator stats displayed on stdout

void vlad_stats(void)
{
   if (DEBUGGING) printf("CALLED VLAD_STATS\n");
   // put whatever code you think will help you
   // understand Vlad's current state in this function
   // REMOVE all of these statements when your vlad_malloc() is done
   
   printf("free_list_pointer is %d\n", free_list_ptr);
   
   // printing out addresses of MAGIC_FREE_HEADER
   printf("indexes of MAGIC_FREE: ");
   vaddr_t count = 0;
   vaddr_t *mem_ptr = (vaddr_t *) memory;
   while (count < memory_size){
      if (mem_ptr[count] == MAGIC_FREE){
         printf("%lu, ", count*sizeof(vaddr_t));
      }
      count++;
   }
   printf("\n");
   
   // printing out addresses of MAGIC_ALLOC_HEADER
   printf("indexes of MAGIC_ALLOC: ");
   count = 0;
   while (count < memory_size){
      if (mem_ptr[count] == MAGIC_ALLOC){
         printf("%lu, ", count*sizeof(vaddr_t));
      }
      count++;
   }
   printf("\n");
   
   printf("numFreeRegions = %d, numAllocRegions = %d\n",numRegions(MAGIC_FREE),numRegions(MAGIC_ALLOC));
   
   // next, have a list of all the regions one after the other showing index and size
   count = 0;
   int region_num = 0;  // region number
   u_int32_t tracker = mem_ptr[count];    // to check if current region is free or alloc
   u_int32_t region_start = 0;            // the index counter for the start of the region
   
   free_header_t *actual_header_ptr;      // pointer for checking struct
   vaddr_t actual_header_index;           // verifying index
   u_int32_t actual_size = 0;             // checking actual size stored in struct
   
   // cycling through the memory array
   while (count < memory_size/4){
      
      // if the current index of the array is a MAGIC, then start the region count from zero
      if (mem_ptr[count] == MAGIC_FREE | mem_ptr[count] == MAGIC_ALLOC){
         region_start = count*sizeof(vaddr_t);
         region_num++;
         
         // getting the index and size of the header at mem_ptr[count]
         actual_header_ptr = (free_header_t *) &mem_ptr[count];
         actual_header_index = (vaddr_t) actual_header_ptr - (vaddr_t) memory;
         actual_size = actual_header_ptr->size;
         
      }
      
      // If the next index is a magic, then end the current region count and print the summary
      if (mem_ptr[count+1] == MAGIC_FREE | mem_ptr[count+1] == MAGIC_ALLOC | count == memory_size/4-1){
         u_int32_t region_end = (count+1)*sizeof(vaddr_t);
         u_int32_t region_size = region_end - region_start;
         char *region_type = (tracker == MAGIC_FREE) ? "Free " : "Alloc";
         
         // printing out the summary
         printf("Region %d: %s : (%4d - %4d) \t Size: %4d \t Actual size: %4d\n",
                region_num, region_type, region_start, region_end, region_size, actual_size);
         
         // update the tracker for the next region
         tracker = mem_ptr[count+1];
      }

      count++;
   }
   
   // printing address of free regions
   printf("free region addresses along next: ");
   vaddr_t curr = free_list_ptr;
   do {
      printf("%d -> ", curr);
      free_header_t *free_header = (free_header_t *) (memory + curr);
      curr = free_header->next;
   } while (curr != free_list_ptr);
   
   printf("%d\nfree region addresses along prev: ", curr);
   curr = free_list_ptr;
   do {
      printf("%d -> ", curr);
      free_header_t *free_header = (free_header_t *) (memory + curr);
      curr = free_header->prev;
   } while (curr != free_list_ptr);
   printf("%d\n", curr);
   
   printf("\n");

   return;
}

// Helper function which returns 1 is num is a sole power of 2 and
// returns 0 if it is not
int isPowerOf2(int num){
   int powerTest = num;
   while (powerTest > 1 && powerTest % 2 == 0){
      powerTest = powerTest/2;
   }
   return (powerTest==1);
}

int numRegions(u_int32_t magic){
   int numFree = 0;
   vaddr_t count = 0;
   vaddr_t *mem_ptr = (vaddr_t *) memory;
   while (count < memory_size){
      if (mem_ptr[count] == magic){
         numFree++;
      }
      count++;
   }
   return numFree;
}