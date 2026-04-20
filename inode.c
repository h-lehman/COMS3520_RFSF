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

//------ your implementation -------

}


//to free an inode with provided inode_number
void free_inode(int inode_number){

//------ your implementation -------

}