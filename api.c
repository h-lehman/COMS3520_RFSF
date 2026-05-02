/*
    implementation of API
*/

#include "def.h"

pthread_mutex_t mutex_for_fs_stat;//mutex used by RSFS_stat()


//initialize file system - should be called as the first thing before accessing this file system 
int RSFS_init(){

    // initialize the mutex used by RSFS_stat()
    pthread_mutex_init(&mutex_for_fs_stat, NULL);

    // initialize data block bitmap and allocate each data block from heap
    pthread_mutex_init(&data_bitmap_mutex, NULL);
    for (int i = 0; i < NUM_DBLOCKS; i++) {
        data_bitmap[i] = 0;                          // all blocks free
        data_blocks[i] = malloc(BLOCK_SIZE);         // allocate block memory
        if (data_blocks[i] == NULL) {
            printf("[RSFS_init] fail to allocate memory for data block %d.\n", i);
            return -1;
        }
        memset(data_blocks[i], 0, BLOCK_SIZE);       // zero out the block
    }

    // initialize inode structures and bitmap
    pthread_mutex_init(&inode_bitmap_mutex, NULL);
    pthread_mutex_init(&inodes_mutex, NULL);
    for (int i = 0; i < NUM_INODES; i++) {
        inode_bitmap[i] = 0;
        for (int j = 0; j < NUM_POINTERS; j++) {
            inodes[i].block[j] = -1;
        }
        inodes[i].length = 0;
    }

    // initialize the open file table
    pthread_mutex_init(&open_file_table_mutex, NULL);
    for (int i = 0; i < NUM_OPEN_FILE; i++) {
        open_file_table[i].used = 0;
        pthread_mutex_init(&open_file_table[i].entry_mutex, NULL);
    }

    // initialize the root directory mutex
    pthread_mutex_init(&root_dir_mutex, NULL);

    // allocate the root inode (inode 0 reserved for root directory)
    root_inode_number = allocate_inode();
    if (root_inode_number < 0) {
        printf("[RSFS_init] fail to allocate root inode.\n");
        return -1;
    }

    return 0; // success

}


//create file
//if file does not exist, create the file and return 0;
//if file_name already exists, return -1; 
//otherwise (other errors), return -2.
int RSFS_create(char file_name){

    // check if file already exists in the directory
    struct dir_entry *entry = search_dir(file_name);
    if (entry != NULL) {
        printf("[RSFS_create] file %c already exists.\n", file_name);
        return -1;
    }

    // allocate a new inode for this file
    int inode_number = allocate_inode();
    if (inode_number < 0) {
        printf("[create] fail to allocate an inode.\n");
        return -2;
    }

    // insert a directory entry mapping file_name -> inode_number
    entry = insert_dir(file_name, inode_number);
    if (entry == NULL) {
        printf("[RSFS_create] fail to insert dir entry for file %c.\n", file_name);
        free_inode(inode_number);
        return -2;
    }

    return 0; // success

}


//open a file with RSFS_RDONLY or RSFS_RDWR flags
//return a file descriptor if succeed;
//otherwise return a negative integer value
int RSFS_open(char file_name, int access_flag){

    // look up the file in the directory
    struct dir_entry *entry = search_dir(file_name);
    if (entry == NULL) {
        printf("[RSFS_open] fail to find file with name: %c\n", file_name);
        return -1;
    }

    int inode_number = (unsigned char)entry->inode_number;
    struct inode *file_inode = &inodes[inode_number];

    // wait until access is permitted (readers-writers protocol)
    pthread_mutex_lock(&file_inode->rw_mutex);
    if (access_flag == RSFS_RDONLY) {
        // readers block only while a writer holds the file
        while (file_inode->is_writing) {
            pthread_cond_wait(&file_inode->rw_cond, &file_inode->rw_mutex);
        }
        file_inode->num_readers++;
    } else {
        // writer blocks until no readers and no other writer hold the file
        while (file_inode->is_writing || file_inode->num_readers > 0) {
            pthread_cond_wait(&file_inode->rw_cond, &file_inode->rw_mutex);
        }
        file_inode->is_writing = 1;
    }
    pthread_mutex_unlock(&file_inode->rw_mutex);

    // allocate an open file entry (fd) initialized with the access flag and inode
    int fd = allocate_open_file_entry(access_flag, inode_number);
    if (fd < 0) {
        printf("[RSFS_open] fail to allocate open file entry for file %c.\n", file_name);
        // roll back readers-writers state so other waiting threads are not blocked
        pthread_mutex_lock(&file_inode->rw_mutex);
        if (access_flag == RSFS_RDONLY) {
            file_inode->num_readers--;
        } else {
            file_inode->is_writing = 0;
        }
        pthread_cond_broadcast(&file_inode->rw_cond);
        pthread_mutex_unlock(&file_inode->rw_mutex);
        return -2;
    }

    return fd;

}



//append the content in buf to the end of the file of descriptor fd
//return the number of bytes actually appended to the file
int RSFS_append(int fd, void *buf, int size){
    
    // validate fd
    if (fd < 0 || fd >= NUM_OPEN_FILE || open_file_table[fd].used == 0) {
        printf("[RSFS_append] invalid fd: %d.\n", fd);
        return -1;
    }

    struct open_file_entry *file_entry = &open_file_table[fd];

    // only RDWR files can be appended to
    if (file_entry->access_flag != RSFS_RDWR) {
        printf("[RSFS_append] file not opened for writing.\n");
        return -1;
    }

    struct inode *file_inode = &inodes[file_entry->inode_number];

    int bytes_appended = 0;
    int write_pos = file_inode->length; // start writing at end of file

    for (int i = 0; i < size; i++) {
        int block_index  = write_pos / BLOCK_SIZE;  // which inode block slot
        int block_offset = write_pos % BLOCK_SIZE;  // byte offset within block

        // cannot exceed max data blocks per file
        if (block_index >= NUM_POINTERS) break;

        // if this block slot has no data block yet, allocate one
        if (file_inode->block[block_index] < 0) {
            int block_num = allocate_data_block();
            if (block_num < 0) {
                printf("[RSFS_append] no free data blocks available.\n");
                break;
            }
            file_inode->block[block_index] = block_num;
        }

        // write one byte from buf into the appropriate block
        char *block_ptr = (char *)data_blocks[file_inode->block[block_index]];
        block_ptr[block_offset] = ((char *)buf)[i];

        write_pos++;
        bytes_appended++;
        file_inode->length++;  // update file length with each byte written
    }

    return bytes_appended;

}



//update current position of the file (which is in the open_file_entry) to offset
//return -1 if fd is invalid; otherwise return the current position after the update
int RSFS_fseek(int fd, int offset){

    // validate fd
    if (fd < 0 || fd >= NUM_OPEN_FILE || open_file_table[fd].used == 0) {
        printf("[RSFS_fseek] invalid fd: %d.\n", fd);
        return -1;
    }

    struct open_file_entry *file_entry = &open_file_table[fd];
    struct inode *file_inode = &inodes[file_entry->inode_number];

    // offset must be within valid range [0, file_length]
    // (allowing seek to length lets reads return 0 bytes at EOF gracefully)
    if (offset < 0 || offset > file_inode->length) {
        printf("[RSFS_fseek] offset %d out of range (file length=%d).\n", 
               offset, file_inode->length);
        return -1;
    }

    file_entry->position = offset;

    return file_entry->position;

}



//read up to size bytes to buf from file's current position towards the end
//return -1 if fd is invalid; otherwise return the number of bytes actually read
int RSFS_read(int fd, void *buf, int size){

    // validate fd
    if (fd < 0 || fd >= NUM_OPEN_FILE || open_file_table[fd].used == 0) {
        printf("[RSFS_read] invalid fd: %d.\n", fd);
        return -1;
    }

    struct open_file_entry *file_entry = &open_file_table[fd];
    struct inode *file_inode = &inodes[file_entry->inode_number];

    // clamp size to bytes remaining from current position to end of file
    int bytes_to_read = size;
    if (file_entry->position + bytes_to_read > file_inode->length) {
        bytes_to_read = file_inode->length - file_entry->position;
    }

    int bytes_read = 0;

    for (int i = 0; i < bytes_to_read; i++) {
        int read_pos     = file_entry->position;
        int block_index  = read_pos / BLOCK_SIZE;   // which inode block slot
        int block_offset = read_pos % BLOCK_SIZE;   // byte offset within that block

        if (block_index >= NUM_POINTERS || file_inode->block[block_index] < 0) {
            break; // no more blocks to read
        }

        // copy one byte from the data block into the output buffer
        char *block_ptr = (char *)data_blocks[file_inode->block[block_index]];
        ((char *)buf)[i] = block_ptr[block_offset];

        file_entry->position++;
        bytes_read++;
    }

    // null-terminate the buffer if space allows (helps treat content as a C string)
    if (bytes_read < size) {
        ((char *)buf)[bytes_read] = '\0';
    }

    return bytes_read;

}


//write the content of size (bytes) in buf to the file (of descripter fd) 
//writing starts at the current position and truncates everything after the write
int RSFS_write(int fd, void *buf, int size){

    // validate fd
    if (fd < 0 || fd >= NUM_OPEN_FILE || open_file_table[fd].used == 0) {
        printf("[RSFS_write] invalid fd: %d.\n", fd);
        return -1;
    }

    struct open_file_entry *file_entry = &open_file_table[fd];

    // write requires RDWR access
    if (file_entry->access_flag != RSFS_RDWR) {
        printf("[RSFS_write] file not opened for writing.\n");
        return -1;
    }

    struct inode *file_inode = &inodes[file_entry->inode_number];

    int bytes_written = 0;
    int write_pos = file_entry->position; // start writing from current position

    for (int i = 0; i < size; i++) {
        int block_index  = write_pos / BLOCK_SIZE;
        int block_offset = write_pos % BLOCK_SIZE;

        // cannot exceed max blocks per file
        if (block_index >= NUM_POINTERS) break;

        // allocate a data block for this slot if not yet allocated
        if (file_inode->block[block_index] < 0) {
            int block_num = allocate_data_block();
            if (block_num < 0) {
                printf("[RSFS_write] no free data blocks.\n");
                break;
            }
            file_inode->block[block_index] = block_num;
        }

        // write one byte into the block
        char *block_ptr = (char *)data_blocks[file_inode->block[block_index]];
        block_ptr[block_offset] = ((char *)buf)[i];

        write_pos++;
        bytes_written++;
    }

    // update position
    file_entry->position = write_pos;

    // RSFS_write truncates: anything after the new write endpoint is discarded
    // update file length to be exactly write_pos (end of new content)
    if (write_pos < file_inode->length) {
        // free data blocks that are now beyond the new file length
        int new_last_block = (write_pos - 1) / BLOCK_SIZE; // last block still in use
        if (write_pos == 0) new_last_block = -1;

        for (int b = new_last_block + 1; b < NUM_POINTERS; b++) {
            if (file_inode->block[b] >= 0) {
                // zero out the freed block for cleanliness
                memset(data_blocks[file_inode->block[b]], 0, BLOCK_SIZE);
                free_data_block(file_inode->block[b]);
                file_inode->block[b] = -1;
            }
        }

        // also zero out bytes in the last used block that are beyond write_pos
        if (write_pos > 0 && new_last_block >= 0 && file_inode->block[new_last_block] >= 0) {
            int last_block_used_bytes = write_pos % BLOCK_SIZE;
            if (last_block_used_bytes > 0) {
                char *block_ptr = (char *)data_blocks[file_inode->block[new_last_block]];
                memset(block_ptr + last_block_used_bytes, 0, BLOCK_SIZE - last_block_used_bytes);
            }
        }
    }

    // set new file length to the write end position
    file_inode->length = write_pos;

    return bytes_written;

}



//close file: return 0 if succeed; otherwise return -1
int RSFS_close(int fd){

    // validate fd
    if (fd < 0 || fd >= NUM_OPEN_FILE || open_file_table[fd].used == 0) {
        printf("[RSFS_close] invalid fd: %d.\n", fd);
        return -1;
    }

    // capture before freeing — free_open_file_entry zeroes the entry
    int inode_number = open_file_table[fd].inode_number;
    int access_flag  = open_file_table[fd].access_flag;

    // release the open file entry back to the table
    free_open_file_entry(fd);

    // update readers-writers state and wake any threads waiting to open this file
    struct inode *file_inode = &inodes[inode_number];
    pthread_mutex_lock(&file_inode->rw_mutex);
    if (access_flag == RSFS_RDONLY) {
        file_inode->num_readers--;
    } else {
        file_inode->is_writing = 0;
    }
    pthread_cond_broadcast(&file_inode->rw_cond);
    pthread_mutex_unlock(&file_inode->rw_mutex);

    return 0;

}



//delete file
int RSFS_delete(char file_name){

    // find the file's directory entry
    struct dir_entry *entry = search_dir(file_name);
    if (entry == NULL) {
        printf("[RSFS_delete] file %c not found.\n", file_name);
        return -1;
    }

    int inode_number = entry->inode_number;
    struct inode *file_inode = &inodes[inode_number];

    // free all data blocks used by this file
    for (int i = 0; i < NUM_POINTERS; i++) {
        if (file_inode->block[i] >= 0) {
            memset(data_blocks[file_inode->block[i]], 0, BLOCK_SIZE); // zero block
            free_data_block(file_inode->block[i]);
            file_inode->block[i] = -1;
        }
    }

    // free the inode
    free_inode(inode_number);

    // remove the directory entry
    delete_dir(file_name);

    return 0;

}



//print status of the file system
// - current status of the file system: File Name, Length, iNode #
// - usage of data blocks: Total Data Blocks, Used, Unused
// - usage of inodes: Total iNode Blocks, Used, Unused
// - # of total opened files
void RSFS_stat(){

    pthread_mutex_lock(&mutex_for_fs_stat);

    printf("\nCurrent status of the file system:\n\n");
    printf("%12s %8s %9s\n", "File Name", "Length", "iNode #");

    // walk the directory entries stored in root's data block to list all files
    // root_data_block is set up by dir.c after the first search_dir call
    if (root_data_block != NULL) {
        int max_entries = BLOCK_SIZE / sizeof(struct dir_entry);
        for (int i = 0; i < max_entries; i++) {
            struct dir_entry *de = (struct dir_entry *)root_data_block + i;
            if (de->name != 0) { // non-empty entry
                int inum = (unsigned char)de->inode_number;
                printf("%12c %8d %9d\n", de->name, inodes[inum].length, inum);
            }
        }
    }

    // count used/unused data blocks
    int data_used = 0;
    for (int i = 0; i < NUM_DBLOCKS; i++) {
        if (data_bitmap[i] == 1) data_used++;
    }

    // count used/unused inodes
    int inode_used = 0;
    for (int i = 0; i < NUM_INODES; i++) {
        if (inode_bitmap[i] == 1) inode_used++;
    }

    // count currently open files
    int open_count = 0;
    for (int i = 0; i < NUM_OPEN_FILE; i++) {
        if (open_file_table[i].used == 1) open_count++;
    }

    printf("\nTotal Data Blocks: %4d,  Used: %d,  Unused: %d\n",
           NUM_DBLOCKS, data_used, NUM_DBLOCKS - data_used);
    printf("Total iNode Blocks: %3d,  Used: %d,  Unused: %d\n",
           NUM_INODES, inode_used, NUM_INODES - inode_used);
    printf("Total Opened Files: %3d\n\n", open_count);

    pthread_mutex_unlock(&mutex_for_fs_stat);

}