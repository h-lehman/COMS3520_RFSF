/*
    allocation of global open_file_table and its guarding mutex; 
    routines for open file entry
*/

#include "def.h"

struct open_file_entry open_file_table[NUM_OPEN_FILE];
pthread_mutex_t open_file_table_mutex;


//allocate an available entry in open file table and return fd (file descriptor);
//return -1 if no entry is found
int allocate_open_file_entry(int access_flag, int inode_number){
    
//------ your implementation -------

}


void free_open_file_entry(int fd){

//------ your implementation -------

}