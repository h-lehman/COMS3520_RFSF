# RSFS - Ridiculously Simple File System

**Author:** Henry Lehman

## Project Description

RSFS (Ridiculously Simple File System) is a multi-threaded, in-memory file system implemented as part of the COMS 3520 (Operating Systems) curriculum. It provides a simplified yet functional model of how a file system manages metadata (inodes), directory structures, and data blocks.

The system supports standard file operations including:

- **Creation and Deletion:** Managing files within a root directory.
- **Open and Close:** Tracking active files via an internal Open File Table.
- **Read and Write:** Performing byte-level operations with support for appending and truncation.
- **Seeking:** Moving the file pointer to specific offsets within a file.

RSFS is designed to be thread-safe, utilizing POSIX threads (pthreads) and mutexes to protect global data structures such as bitmaps and the open file table.

## Project Files

The following files were modified to complete the RSFS implementation:

- **api.c**: Implements the main file system API (e.g., `RSFS_create`, `RSFS_open`, `RSFS_read`, `RSFS_write`, `RSFS_delete`).
- **inode.c**: Manages inode allocation and initialization via an inode bitmap.
- **open_file_table.c**: Manages the system-wide table of open files, tracking file descriptors, offsets, and access modes.
