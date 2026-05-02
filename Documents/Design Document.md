# RSFS Design Document

### COMS 3520 – Project 2

**Author:** Henry Lehman

---

## Overview

Initial design notes for each function required in the RSFS (Ridiculously Simple File System) project. Each section includes a brief analogy, high-level pseudocode drafted before writing any real code, followed by a list of things that needed to be thought through more carefully during actual implementation.

**The running analogy:** RSFS is a desk drawer. Files are papers in the drawer. Data blocks are the individual sections of the drawer where text is physically stored. Inodes are index cards listing which sections hold each file. Bitmaps are checklists tracking which sections are in use. The open file table is a box of bookmarks — one per open file — tracking where each active reader or writer left off.

---

## Part 1: Open File Table Functions (`open_file_table.c`)

### `allocate_open_file_entry(int access_flag, int inode_number)`

**Purpose:** Find an unused slot in the open file table, fill in the fields, and return its index as the file descriptor.

**Analogy:** You have a box of exactly 8 bookmarks. When someone opens a file, grab the first unused bookmark, write the file's ID card number (inode) and reading/writing mode on it, and set it at position 0 (the front of the book). Hand the bookmark's number back as the file descriptor.

#### Initial Design

```
function allocate_open_file_entry(access_flag, inode_number):
    for i from 0 to NUM_OPEN_FILE:
        if  open_file_table[i].used == 0:
            open_file_table[i].used = 1
            open_file_table[i].inode_number = inode_number
            open_file_table[i].access_flag = access_flag
            open_file_table[i].position = 0
            return i
    return -1   // no free slot
```

The basic idea is straightforward — loop through the table and grab the first unused entry. Fill in all the known fields and return the index, which acts as the file descriptor. Return -1 if no slot is available.

**Things to add or consider during implementation:**

- Lock/unlock `open_file_table_mutex` around the search and assignment
- Initialize `entry_mutex` for the allocated slot
- Confirm initial value for `position`
- Break out of the loop once a slot is found

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `free_open_file_entry(int fd)`

**Purpose:** Mark the entry at index `fd` as no longer in use.

**Analogy:** The person is done with the file. They hand the bookmark back, you erase it, and toss it back in the box so someone else can use it.

#### Initial Design

```
function free_open_file_entry(fd):
    open_file_table[fd].used = 0
```

Seems simple — just mark the entry as free so it can be reused later. Since `allocate_open_file_entry` overwrites all the fields on the next allocation, resetting everything here may not be strictly necessary.

**Things to add or consider during implementation:**

- Lock/unlock `open_file_table_mutex`
- Decide whether other fields need to be reset or if flipping `used` is enough
- Decide whether to do anything with `entry_mutex` on free

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

## Part 2: Inode Functions (`inode.c`)

### `allocate_inode()`

**Purpose:** Find a free inode in the bitmap, mark it as used, and return its index.

**Analogy:** You have 8 blank ID cards. To create a new file, scan the ledger (inode_bitmap) for an unchecked slot, check it off, and initialize the card — no drawer sections assigned yet, length zero.

#### Initial Design

```
function allocate_inode():
    for i from 0 to NUM_INODES:
        if inode_bitmap[i] == 0:
            inode_bitmap[i] = 1
            inodes[i].length = 0
            inodes[i].block[] = all -1   // -1 means no drawer section assigned
            return i
    return -1   // no free inodes
```

Works the same way as the data block bitmap — scan for a 0, flip it to 1, initialize the inode fields, and return the index. Return -1 if no free inode is found.

**Things to add or consider during implementation:**

- Lock/unlock `inode_bitmap_mutex`
- Initialize block pointer array to -1 (`char` type, so -1 works as the sentinel)
- Initialize `length` to 0
- Break out of the loop after finding a slot

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `free_inode(int inode_number)`

**Purpose:** Mark the inode at `inode_number` as free in the bitmap and reset its fields.

**Analogy:** You are tearing up a file's ID card. Erase everything written on it, reset all the fields to blank, and mark the slot as available in the ledger.

#### Initial Design

```
function free_inode(inode_number):
    inode_bitmap[inode_number] = 0
    inodes[inode_number].length = 0
    inodes[inode_number].block[] = all -1
```

Just the reverse of allocate — flip the bit back to 0 and wipe the fields so the slot is clean for reuse. The caller is responsible for freeing any data blocks before calling this.

**Things to add or consider during implementation:**

- Lock/unlock `inode_bitmap_mutex`
- Reset block pointers and `length` so the slot is clean for reuse
- Confirm that data blocks are freed by the caller before this is called, not here

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

## Part 3: API Functions (`api.c`)

### `RSFS_init()`

**Purpose:** Initialize all the data structures the file system needs before any other API function is called.

**Analogy:** First day at the office. Set up the desks (initialize mutexes), allocate and zero out every physical drawer section (malloc the data blocks), wipe the maps and ledgers clean (zero bitmaps), clear the bookmark box (open file table), and create the one Master Drawer the whole system will use (root inode).

#### Initial Design

```
function RSFS_init():
    initialize all mutexes

    for each data block i:
        data_bitmap[i] = 0
        data_blocks[i] = malloc(BLOCK_SIZE)
        memset(data_blocks[i], 0, BLOCK_SIZE)

    for each inode i:
        inode_bitmap[i] = 0
        inodes[i].length = 0
        inodes[i].block[] = all -1

    for each open file entry i:
        open_file_table[i].used = 0
        initialize entry_mutex

    root_inode_number = allocate_inode()
    return 0
```

Go through each data structure and put it in a known starting state before anything else runs.

**Things to add or consider during implementation:**

- Initialize all mutexes before anything that uses them is called
- `data_blocks` are pointers — each one needs `malloc(BLOCK_SIZE)` from the heap
- Return -1 if any memory allocation fails
- The root directory's data block is allocated lazily by `dir.c` on the first `search_dir` call — just allocating the root inode here is enough

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_create(char file_name)`

**Purpose:** Create a new empty file with the given name. Return 0 on success, -1 if the name already exists, -2 on other errors.

**Analogy:** A customer asks for a new file named 'A'. Check the Master Drawer for a sticker with that name — if one exists, refuse. Otherwise grab a blank ID card (inode), write a new sticker 'A → inode #N', and slip it into the Master Drawer.

#### Initial Design

```
function RSFS_create(file_name):
    entry = search_dir(file_name)
    if entry != NULL:
        return -1   // file already exists

    inode_number = allocate_inode()
    if inode_number < 0:
        return -2   // no free inodes

    entry = insert_dir(file_name, inode_number)
    if entry == NULL:
        free_inode(inode_number)   // clean up the allocated inode
        return -2

    return 0
```

First check if the file already exists, then grab a new inode for it, then add a directory entry to map the name to the inode. Free the inode and return an error if `insert_dir` fails.

**Things to add or consider during implementation:**

- Check return value of `insert_dir` and free inode if it fails
- Confirm the exact arguments `insert_dir` expects
- Handle the case where the directory is full

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_open(char file_name, int access_flag)`

**Purpose:** Open an existing file and return a file descriptor (index into the open file table).

**Analogy:** Customer says "I want file 'A'." Find the sticker for 'A' to get its inode number, grab a bookmark from the box, write the inode number and access mode on it, set it to position 0, and hand the bookmark's index back as the file descriptor.

#### Initial Design

```
function RSFS_open(file_name, access_flag):
    entry = search_dir(file_name)
    if entry == NULL:
        return -1

    fd = allocate_open_file_entry(access_flag, entry->inode_number)
    if fd < 0:
        return -2

    return fd
```

Look up the file in the directory, then grab a slot in the open file table for it. The fd returned is the index of that slot.

**Things to add or consider during implementation:**

- Check for sign extension when reading `inode_number` out of the dir entry (`char` type)
- For advanced part: block if a conflicting opener already has the file open
- For advanced part: add reader/writer counting logic per inode

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_append(int fd, void *buf, int size)`

**Purpose:** Append `size` bytes from `buf` to the end of the file. Return the number of bytes actually appended.

**Analogy:** You want to add text to the very end of the paper. Look at the ID card to find where the file ends. If the last drawer section has room, write there. If it's full, grab a new drawer section, record its number on the ID card, and then write.

#### Initial Design

```
function RSFS_append(fd, buf, size):
    if fd is invalid or not opened RDWR:
        return -1

    write_pos = inode->length    // start writing at end of file

    for i from 0 to size:
        block_index  = write_pos / BLOCK_SIZE
        block_offset = write_pos % BLOCK_SIZE

        if block_index >= NUM_POINTERS: break    // at max file size

        if inode->block[block_index] < 0:        // no block assigned yet
            block_num = allocate_data_block()
            if block_num < 0: break              // out of space
            inode->block[block_index] = block_num

        data_blocks[inode->block[block_index]][block_offset] = buf[i]
        write_pos++
        inode->length++                          // update length per byte written

    return bytes_appended
```

Start writing at the end of the file (`inode->length`) and work forward byte by byte. A new data block gets allocated whenever the write crosses into a new block slot. `inode->length` is incremented inside the loop for each byte successfully written.

**Things to add or consider during implementation:**

- Block index: `write_pos / BLOCK_SIZE`, offset within block: `write_pos % BLOCK_SIZE`
- Stop early if `block_index >= NUM_POINTERS` (file is at max size)
- Stop early and return partial count if `allocate_data_block()` fails
- Update `inode->length` inside the loop, not after — so a partial append still updates length correctly
- Validate `fd` range and `used` flag before anything else

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_fseek(int fd, int offset)`

**Purpose:** Set the current read/write position of the file to `offset`. Return the new position, or -1 on error.

**Analogy:** Move the bookmark to a specific character number. Can't move it before the start (< 0) or past the end of the file.

#### Initial Design

```
function RSFS_fseek(fd, offset):
    if fd is invalid:
        return -1

    if offset < 0 or offset > inode->length:
        return -1

    open_file_table[fd].position = offset
    return offset
```

Validate the fd and the offset, then update the position field in the open file entry. The main thing to nail down is the exact valid range.

**Things to add or consider during implementation:**

- Look up inode to get `inode->length` for the bounds check
- Valid range is `[0, inode->length]` — seeking to exactly `length` is allowed; a subsequent read will just return 0 bytes
- Validate `fd` is in range and `used == 1` before accessing the inode

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_read(int fd, void *buf, int size)`

**Purpose:** Read up to `size` bytes from the file at the current position into `buf`. Advance position by bytes read. Return bytes read, or -1 on invalid fd.

**Analogy:** Starting from where the bookmark is, read characters one by one. Check the ID card each time to find which drawer section holds the current character. Move the bookmark forward for every byte read.

#### Initial Design

```
function RSFS_read(fd, buf, size):
    if fd is invalid:
        return -1

    clamp size to (inode->length - position)

    for i from 0 to clamped_size:
        block_index  = position / BLOCK_SIZE
        block_offset = position % BLOCK_SIZE

        if block_index >= NUM_POINTERS or inode->block[block_index] < 0: break

        buf[i] = data_blocks[inode->block[block_index]][block_offset]
        position++
        bytes_read++

    if bytes_read < size:
        buf[bytes_read] = '\0'   // null-terminate so content can be treated as a C string

    return bytes_read
```

Start at the current position and read forward byte by byte, computing the block and offset each iteration. Stop at the requested size or end of file, whichever comes first.

**Things to add or consider during implementation:**

- Clamp `size` to `inode->length - position` before the loop
- Block index: `position / BLOCK_SIZE`, offset: `position % BLOCK_SIZE`
- Increment `file_entry->position` inside the loop, not after
- Guard against a block pointer that is -1 (unallocated)
- Null-terminate the buffer if `bytes_read < size` — helps treat result as a C string

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_write(int fd, void *buf, int size)`

**Purpose:** Write `size` bytes from `buf` into the file starting at the current position. Truncate everything after the write endpoint. Return bytes written, or -1 on error.

**Analogy:** Overwrite whatever is on the paper starting from the bookmark. After writing, tear off everything after where the pen stopped. If you write past the old end of the paper, the paper just gets longer instead of being truncated.

#### Initial Design

```
function RSFS_write(fd, buf, size):
    if fd is invalid or not opened RDWR:
        return -1

    write_pos = current position

    for i from 0 to size:
        block_index  = write_pos / BLOCK_SIZE
        block_offset = write_pos % BLOCK_SIZE
        if block_index >= NUM_POINTERS: break
        if inode->block[block_index] < 0: allocate new data block
        data_blocks[...][block_offset] = buf[i]
        write_pos++

    update position to write_pos

    // truncation: only runs if write_pos < old file length
    if write_pos < old_length:
        for each block slot b beyond write_pos:
            zero out data_blocks[inode->block[b]]
            free_data_block(inode->block[b])
            inode->block[b] = -1
        zero out bytes in last partial block beyond write_pos

    inode->length = write_pos    // new file length is the write end position
    return bytes_written
```

Similar to append but writing from the current position instead of the end. After the write, the file ends at `write_pos`. If `write_pos > old_length` the file grows; if `write_pos < old_length` blocks beyond the new end are freed.

**Things to add or consider during implementation:**

- Truncation only fires when `write_pos < old_length` — no-op if the write grows the file
- Zero out bytes in the last partial block beyond `write_pos % BLOCK_SIZE`
- Handle `write_pos == 0` as a special case: `new_last_block = -1`, so all blocks get freed
- Validate `fd` and `access_flag` before starting

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_close(int fd)`

**Purpose:** Close the file by releasing its open file table entry. Return 0 on success, -1 on invalid fd.

**Analogy:** Take the bookmark back, erase it, and return it to the box so someone else can use it.

#### Initial Design

```
function RSFS_close(fd):
    if fd is invalid:
        return -1

    free_open_file_entry(fd)
    return 0
```

Closing a file means freeing its slot in the open file table. Validate the fd first, then hand off to `free_open_file_entry`.

**Things to add or consider during implementation:**

- Validate `fd` is in range and `used == 1` before freeing
- For advanced part: call `pthread_cond_broadcast` after freeing so waiting threads can retry
- Nothing to flush since the FS is in-memory

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_delete(char file_name)`

**Purpose:** Delete the file with the given name. Free all its data blocks, its inode, and its directory entry.

**Analogy:** Find the sticker for the file. Look at the ID card to see which drawer sections it uses. Empty and reclaim each of those sections. Tear up the ID card. Remove the sticker from the Master Drawer. Order matters — clear the drawer sections before tearing the card.

#### Initial Design

```
function RSFS_delete(file_name):
    entry = search_dir(file_name)
    if entry == NULL:
        return -1

    inode_number = entry->inode_number
    inode = &inodes[inode_number]

    // free all data blocks used by this file
    for b from 0 to NUM_POINTERS:
        if inode->block[b] >= 0:
            zero out data_blocks[inode->block[b]]
            free_data_block(inode->block[b])
            inode->block[b] = -1

    free_inode(inode_number)
    delete_dir(file_name)

    return 0
```

Find the file in the directory, free all its data blocks (so no dangling block references remain), then free the inode, then remove the directory entry.

**Things to add or consider during implementation:**

- Free data blocks before calling `free_inode`, not after
- Zero out each freed data block so old data doesn't linger
- Check each block pointer against -1 before trying to free it
- Order matters: blocks → inode → directory entry

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

### `RSFS_stat()`

**Purpose:** Print the current state of the file system: list of files with name/length/inode, data block usage, inode usage, and number of open files.

**Analogy:** Walk the office and take inventory. Count every sticker in the Master Drawer (files), every occupied drawer section (data blocks), every checked-off ledger slot (inodes), and every bookmark still in use (open file table).

#### Initial Design

```
function RSFS_stat():
    lock mutex_for_fs_stat

    print header

    if root_data_block != NULL:
        for each entry in root_data_block:
            if entry.name != 0:
                print name, inode->length, inode_number

    count used data blocks from data_bitmap
    count used inodes from inode_bitmap
    count open files from open_file_table

    print summary

    unlock mutex_for_fs_stat
```

Walk through the root directory block and print each valid file entry, then tally up the bitmaps and open file table for the usage summary.

**Things to add or consider during implementation:**

- Cast `root_data_block` to `struct dir_entry *` to iterate entries
- Number of entries per block: `BLOCK_SIZE / sizeof(struct dir_entry)`
- Skip entries where `name == 0`
- Cast `inode_number` to `unsigned char` when using as an array index to avoid sign extension
- Guard the directory walk with `if (root_data_block != NULL)` — `dir.c` allocates it lazily on the first `search_dir` call

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

## Part 4: Conversion to Advanced (Readers-Writers)

This section covers the changes needed to make the file system support concurrent readers and exclusive writers. The logic is isolated to three places: the inode struct definition, `allocate_inode`/`free_inode`, and `RSFS_open`/`RSFS_close`.

---

### Struct Changes (`def.h` and `inode.c`)

**Purpose:** Add per-inode state to track how many readers currently have the file open and whether a writer holds it, along with the synchronization primitives threads need to wait on.

**Analogy:** Each ID card (inode) gets a small sign holder attached to it. The sign holder tracks how many blue "reading" signs are posted and whether a red "writing" sign is up. It also has a waiting area (condition variable) where threads park themselves if they can't open the file yet.

#### Initial Design

```
// added to struct inode in def.h:
int num_readers      // how many readers currently have this file open
int is_writing       // 1 if a writer holds it, 0 otherwise
pthread_mutex_t rw_mutex   // protects num_readers and is_writing
pthread_cond_t  rw_cond    // threads wait here when access is blocked

// in allocate_inode(), inside the if block after marking bitmap:
inodes[i].num_readers = 0
inodes[i].is_writing  = 0
pthread_mutex_init(&inodes[i].rw_mutex, NULL)
pthread_cond_init(&inodes[i].rw_cond, NULL)

// in free_inode(), before unlocking:
pthread_mutex_destroy(&inodes[inode_number].rw_mutex)
pthread_cond_destroy(&inodes[inode_number].rw_cond)
inodes[inode_number].num_readers = 0
inodes[inode_number].is_writing  = 0
```

Initialize the new fields inside `allocate_inode` where the rest of the inode is already being set up, and destroy them in `free_inode` before resetting the fields.

**Things to add or consider during implementation:**

- Init and destroy belong in `allocate_inode`/`free_inode`, not in `RSFS_init` — every inode goes through those functions before it can be used, so `RSFS_init` does not need to touch these fields
- Destroy before zeroing, not after — you can't destroy a mutex you've already zeroed
- `rw_mutex` and `rw_cond` are per-inode, not global — no deadlock risk with the bitmap mutex that already wraps the init/destroy calls

---

### `RSFS_open` — Advanced Version

**Purpose:** Before handing out a file descriptor, block the calling thread if the current state of the inode does not permit the requested access mode.

**Analogy:** Before grabbing a bookmark, check the sign holder. Readers wait if a red sign (writer) is up. Writers wait if any sign at all is up. Once it's clear, post your sign and proceed.

#### Initial Design

```
function RSFS_open(file_name, access_flag):
    entry = search_dir(file_name)
    if entry == NULL: return -1

    inode_number = (unsigned char)entry->inode_number
    file_inode = &inodes[inode_number]

    lock file_inode->rw_mutex

    if access_flag == RSFS_RDONLY:
        while file_inode->is_writing:
            cond_wait(&file_inode->rw_cond, &file_inode->rw_mutex)
        file_inode->num_readers++
    else:  // RSFS_RDWR
        while file_inode->is_writing or file_inode->num_readers > 0:
            cond_wait(&file_inode->rw_cond, &file_inode->rw_mutex)
        file_inode->is_writing = 1

    unlock file_inode->rw_mutex

    fd = allocate_open_file_entry(access_flag, inode_number)
    if fd < 0:
        // rollback: undo the state change and wake any waiting threads
        lock file_inode->rw_mutex
        if access_flag == RSFS_RDONLY: file_inode->num_readers--
        else: file_inode->is_writing = 0
        cond_broadcast(&file_inode->rw_cond)
        unlock file_inode->rw_mutex
        return -2

    return fd
```

Lock the inode's rw_mutex, loop until the condition allows entry, update state, then unlock before allocating the fd. If fd allocation fails, roll back the state change and broadcast so no thread gets permanently blocked.

**Things to add or consider during implementation:**

- Use `while`, not `if`, for the cond_wait loop — spurious wakeups are real
- Update `num_readers`/`is_writing` inside the lock before unlocking — no window for a race
- The rollback path needs its own `pthread_cond_broadcast`, not `signal` — you don't know if a reader or a writer is waiting next

---

### `RSFS_close` — Advanced Version

**Purpose:** After freeing the open file entry, update the inode's reader/writer state and wake any threads blocked in `RSFS_open` waiting for this file.

**Analogy:** Take down your sign (decrement readers or clear the writer flag). Then announce to the waiting area that the file might be available now — everyone wakes up and checks again.

#### Initial Design

```
function RSFS_close(fd):
    if fd is invalid: return -1

    // capture before freeing — free_open_file_entry zeroes the entry
    inode_number = open_file_table[fd].inode_number
    access_flag  = open_file_table[fd].access_flag

    free_open_file_entry(fd)

    file_inode = &inodes[inode_number]
    lock file_inode->rw_mutex
    if access_flag == RSFS_RDONLY:
        file_inode->num_readers--
    else:
        file_inode->is_writing = 0
    cond_broadcast(&file_inode->rw_cond)
    unlock file_inode->rw_mutex

    return 0
```

Free the fd slot first, then update the inode state and wake everyone.

**Things to add or consider during implementation:**

- Read `inode_number` and `access_flag` from the table entry **before** calling `free_open_file_entry` — that call zeroes the entry, so those values are gone afterward
- Use `broadcast`, not `signal` — multiple readers may be waiting and all of them should be allowed through at once
- Nothing to flush — the FS is in-memory, so closing is purely about releasing the table entry and updating state

The rest of the development cycle can be seen in the Full Transcript Document and the final code is in the source folder.

---

_End of Design Document_
