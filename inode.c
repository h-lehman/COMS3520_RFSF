/*
    allocation of inodes, inode bitmap, and mutexes to guard them;
    routines for inode management
*/

#include "def.h"


//allocation of inodes, inode bitmap and their mutexes
struct inode inodes[NUM_INODES];
pthread_mutex_t inodes_mutex;
int inode_bitmap[NUM_INODES];
pthread_mutex_t inode_bitmap_mutex;

//root inode number, which should be known globally
int root_inode_number=-1;


//to allocate an empty inode and return the inode-number; 
//if no free inode is available, return -1
int allocate_inode(){

    int inode_number = -1; // default: not found

    pthread_mutex_lock(&inode_bitmap_mutex);

    for (int i = 0; i < NUM_INODES; i++) {
        if (inode_bitmap[i] == 0) {    // found a free inode
            inode_number = i;
            inode_bitmap[i] = 1;       // mark as used

            // initialize the inode: no data blocks assigned, zero length
            for (int j = 0; j < NUM_POINTERS; j++) {
                inodes[i].block[j] = -1; // -1 means block slot is unused
            }
            inodes[i].length = 0;
            // initialize readers-writers state
            inodes[i].num_readers = 0;
            inodes[i].is_writing = 0;
            pthread_mutex_init(&inodes[i].rw_mutex, NULL);
            pthread_cond_init(&inodes[i].rw_cond, NULL);

            break;
        }
    }

    pthread_mutex_unlock(&inode_bitmap_mutex);

    return inode_number;
}


//to free an inode with provided inode_number
void free_inode(int inode_number){

    pthread_mutex_lock(&inode_bitmap_mutex);

    inode_bitmap[inode_number] = 0;  // mark as free

    // reset inode fields so the slot is clean for future use
    for (int j = 0; j < NUM_POINTERS; j++) {
        inodes[inode_number].block[j] = -1;
    }
    inodes[inode_number].length = 0;
    // destroy readers-writers primitives
    pthread_mutex_destroy(&inodes[inode_number].rw_mutex);
    pthread_cond_destroy(&inodes[inode_number].rw_cond);
    inodes[inode_number].num_readers = 0;
    inodes[inode_number].is_writing = 0;

    pthread_mutex_unlock(&inode_bitmap_mutex);
}