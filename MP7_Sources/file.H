/*
     File        : file.H

     Author      : Riccardo Bettati
     Modified    : 2021/11/18

     Description : Simple File class with sequential read/write operations.
 
*/

#ifndef _FILE_H_
#define _FILE_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "file_system.H"

/*--------------------------------------------------------------------------*/
/* FORWARDS */ 
/*--------------------------------------------------------------------------*/

class Inode;
class FileSystem;

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* class  F i l e   */
/*--------------------------------------------------------------------------*/

class File  {
    
private:
    /* -- your file data structures here ... */
    
    /* You will need a reference to the inode, maybe even a reference to the 
       file system. 
       You may also want a current position, which indicates which position in 
       the file you will read or write next. */

    /* ADDED: Pointer to this file's inode (owned by the FileSystem object) */
    Inode* inode;

    /* ADDED: Back-pointer to the FileSystem; needed to access the disk */
    FileSystem* fs;

    /* ADDED: Byte offset of the next read or write within the file */
    unsigned int current_position;
    
    unsigned char block_cache[SimpleDisk::BLOCK_SIZE];
    /* It will be helpful to have a cached copy of the block that you are reading
       from and writing to. In the base submission, files have only one block, which
       you can cache here. You read the data from disk to cache whenever you open the
       file, and you write the data to disk whenever you close the file. 
    */

    /* ADDED (Option 1): Number of logical blocks in the max-size multi-block cache.
       MAX_DIRECT direct blocks + PTRS_PER_INDIRECT indirect blocks = 132 blocks total.
       The full cache is heap-allocated (multi_block_cache) to avoid overflowing the
       8 KB kernel stack; block_cache above is kept for source compatibility. */
    static constexpr unsigned int PTRS_PER_INDIRECT =
        SimpleDisk::BLOCK_SIZE / sizeof(int);  /* 128 */
    static constexpr unsigned int MAX_FILE_BLOCKS =
        Inode::MAX_DIRECT + PTRS_PER_INDIRECT; /* 132 */
    static constexpr unsigned int MAX_FILE_SIZE =
        MAX_FILE_BLOCKS * SimpleDisk::BLOCK_SIZE; /* 67,584 bytes ~ 66 KB */

    /* ADDED (Option 1): Heap-allocated buffer large enough for the entire file.
       Allocated in the constructor, freed in the destructor. */
    unsigned char* multi_block_cache;

    /* ADDED (Option 1): For each logical block slot, track the physical disk
       block number (-1 = not yet allocated). This mirrors the inode's direct and
       indirect tables but is flat for O(1) lookup during read/write. */
    int block_map[MAX_FILE_BLOCKS];

    /* ADDED (Option 1): How many logical block slots are currently in use
       (i.e., allocated on disk). Updated as blocks are lazily allocated. */
    unsigned int num_allocated_blocks;

    /* ADDED (Option 1): Helper — translate a logical block index to a physical
       disk block number, allocating one if necessary. Returns -1 on failure. */
    int get_or_alloc_block(unsigned int logical_block_idx);

    /* ADDED (Option 1): Read the indirect block from disk and populate the
       upper portion of block_map[] (indices MAX_DIRECT .. MAX_FILE_BLOCKS-1). */
    void load_indirect_block();

    /* ADDED (Option 1): Write the upper portion of block_map[] back to the
       indirect block on disk (allocating the indirect block if needed). */
    void save_indirect_block();

public:

    File(FileSystem * _fs, int _id); 
    /* Constructor for the file handle. Set the 'current position' to be at the 
       beginning of the file. */

    ~File();
    /* Closes the file. Deletes any data structures associated with the file handle. */
  
    int Read(unsigned int _n, char * _buf);
    /* Read _n characters from the file starting at the current position and
       copy them in _buf.  Return the number of characters read. 
       Do not read beyond the end of the file. */
    
    int Write(unsigned int _n, const char * _buf);
    /* Write _n characters to the file starting at the current position. If the write
       extends over the end of the file, extend the length of the file until all data is 
       written or until the maximum file size is reached. Do not write beyond the maximum
       length of the file.  
       Return the number of characters written. */
    
    void Reset();
    /* Set the 'current position' to the beginning of the file. */
    
    bool EoF();
    /* Is the current position for the file at the end of the file? */

};

#endif