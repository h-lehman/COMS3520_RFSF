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
    
    int fd = -1; // default: not found

    pthread_mutex_lock(&open_file_table_mutex);

    for (int i = 0; i < NUM_OPEN_FILE; i++) {
        if (open_file_table[i].used == 0) {   // found an unused entry
            fd = i;
            open_file_table[i].used = 1;       // mark as in use
            open_file_table[i].inode_number = inode_number;
            open_file_table[i].access_flag = access_flag;
            open_file_table[i].position = 0;   // start at beginning of file
            pthread_mutex_init(&open_file_table[i].entry_mutex, NULL);
            break;
        }
    }

    pthread_mutex_unlock(&open_file_table_mutex);

    return fd;
}


// free_open_file_entry: marks the entry at index fd as unused
void free_open_file_entry(int fd){

    pthread_mutex_lock(&open_file_table_mutex);

    open_file_table[fd].used = 0;  // mark entry as free

    pthread_mutex_unlock(&open_file_table_mutex);
}