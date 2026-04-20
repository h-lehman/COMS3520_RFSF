/*
    implementation of API
*/

#include "def.h"

pthread_mutex_t mutex_for_fs_stat;//mutex used by RSFS_stat()


//initialize file system - should be called as the first thing before accessing this file system 
int RSFS_init(){

//------ your implementation -------

}


//create file
//if file does not exist, create the file and return 0;
//if file_name already exists, return -1; 
//otherwise (other errors), return -2.
int RSFS_create(char file_name){

//------ your implementation -------

}


//open a file with RSFS_RDONLY or RSFS_RDWR flags
//return a file descriptor if succeed; 
//otherwise return a negative integer value
int RSFS_open(char file_name, int access_flag){
    
//------ your implementation -------

}



//append the content in buf to the end of the file of descriptor fd
//return the number of bytes actually appended to the file
int RSFS_append(int fd, void *buf, int size){
    
//------ your implementation -------

}



//update current position of the file (which is in the open_file_entry) to offset
//return -1 if fd is invalid; otherwise return the current position after the update
int RSFS_fseek(int fd, int offset){

//------ your implementation -------

}



//read up to size bytes to buf from file's current position towards the end
//return -1 if fd is invalid; otherwise return the number of bytes actually read
int RSFS_read(int fd, void *buf, int size){

//------ your implementation -------

}


//write the content of size (bytes) in buf to the file (of descripter fd) 
int RSFS_write(int fd, void *buf, int size){

//------ your implementation -------

}



//close file: return 0 if succeed; otherwise return -1
int RSFS_close(int fd){
    
//------ your implementation -------

}



//delete file
int RSFS_delete(char file_name){

//------ your implementation -------

}



//print status of the file system
// - current status of the file system: File Name, Length, iNode #
// - usage of data blocks: Total Data Blocks, Used, Unused
// - usage of inodes: Total iNode Blocks, Used, Unused
// - # of total opened files
void RSFS_stat(){

//------ your implementation -------

}
