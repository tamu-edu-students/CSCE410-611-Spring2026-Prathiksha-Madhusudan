/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(FileSystem *_fs, int _id) {
    Console::puts("Opening file.\n");
    /* ADDED: Stash references and locate the file's inode */
    fs               = _fs;
    inode            = fs->LookupFile(_id);
    current_position = 0;

    /* ADDED: Zero the block cache so no stale bytes bleed through */
    for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) {
        block_cache[i] = 0;
    }

    /* ADDED (Option 1): Initialise the flat block map to "not allocated" */
    num_allocated_blocks = 0;
    for (unsigned int i = 0; i < MAX_FILE_BLOCKS; i++) {
        block_map[i] = -1;
    }

    /* ADDED (Option 1): Allocate the heap cache that spans all possible blocks */
    multi_block_cache = new unsigned char[MAX_FILE_SIZE];
    for (unsigned int i = 0; i < MAX_FILE_SIZE; i++) {
        multi_block_cache[i] = 0;
    }

    if (inode == nullptr) return;

    /* ADDED (Option 1): Populate block_map[] from the inode's direct entries */
    for (unsigned int d = 0; d < Inode::MAX_DIRECT; d++) {
        block_map[d] = inode->block_no[d];
    }

    /* ADDED (Option 1): Populate block_map[] from the indirect block (if any) */
    load_indirect_block();

    /* ADDED (Option 1): Load every allocated block from disk into the cache */
    for (unsigned int lb = 0; lb < MAX_FILE_BLOCKS; lb++) {
        if (block_map[lb] >= 0) {
            fs->disk->read((unsigned long)block_map[lb],
                           multi_block_cache + lb * SimpleDisk::BLOCK_SIZE);
            num_allocated_blocks++;
        }
    }
}

File::~File() {
    Console::puts("Closing file.\n");
    /* Make sure that you write any cached data to disk. */
    /* Also make sure that the inode in the inode list is updated. */

    if (inode != nullptr) {
        /* ADDED (Option 1): Write every allocated logical block back to disk */
        for (unsigned int lb = 0; lb < MAX_FILE_BLOCKS; lb++) {
            if (block_map[lb] >= 0) {
                fs->disk->write((unsigned long)block_map[lb],
                                multi_block_cache + lb * SimpleDisk::BLOCK_SIZE);
            }
        }

        /* ADDED (Option 1): Persist the indirect block table (if used) */
        save_indirect_block();

        /* ADDED (Option 1): Sync the direct block numbers back to the inode */
        for (unsigned int d = 0; d < Inode::MAX_DIRECT; d++) {
            inode->block_no[d] = block_map[d];
        }
    }

    /* ADDED (Option 1): Release the heap cache */
    if (multi_block_cache != nullptr) {
        delete[] multi_block_cache;
        multi_block_cache = nullptr;
    }

    /* ADDED: Persist the inode table so the updated file_size is saved */
    if (fs != nullptr) {
        fs->save_inodes();
    }
}

/*--------------------------------------------------------------------------*/
/* OPTION 1 HELPER FUNCTIONS */
/*--------------------------------------------------------------------------*/

/* ADDED (Option 1): Read the indirect block from disk into the upper half of
   block_map[] (slots MAX_DIRECT .. MAX_FILE_BLOCKS-1).
   Called once during File::File() after the direct entries are loaded. */
void File::load_indirect_block() {
    if (inode == nullptr || inode->indirect_block_no < 0) return;

    unsigned char ibuf[SimpleDisk::BLOCK_SIZE];
    fs->disk->read((unsigned long)inode->indirect_block_no, ibuf);

    for (unsigned int p = 0; p < PTRS_PER_INDIRECT; p++) {
        /* Deserialise each 4-byte little-endian integer */
        int blk = 0;
        for (unsigned int b = 0; b < sizeof(int); b++) {
            blk |= ((int)(unsigned char)ibuf[p * sizeof(int) + b]) << (b * 8);
        }
        block_map[Inode::MAX_DIRECT + p] = blk;
    }
}

/* ADDED (Option 1): Write the upper half of block_map[] back to the indirect
   block on disk. Allocates the indirect block itself if it hasn't been yet.
   Called once during File::~File() before the inode is saved. */
void File::save_indirect_block() {
    if (inode == nullptr) return;

    /* Check whether any indirect slot is actually in use */
    bool any_indirect = false;
    for (unsigned int p = 0; p < PTRS_PER_INDIRECT; p++) {
        if (block_map[Inode::MAX_DIRECT + p] >= 0) {
            any_indirect = true;
            break;
        }
    }
    if (!any_indirect) return;

    /* Allocate the indirect block on disk if it doesn't exist yet */
    if (inode->indirect_block_no < 0) {
        int ib = fs->get_free_block();
        if (ib < 0) return; /* disk full — best effort */
        inode->indirect_block_no = ib;
    }

    /* Serialise block_map[MAX_DIRECT .. MAX_FILE_BLOCKS-1] into the block */
    unsigned char ibuf[SimpleDisk::BLOCK_SIZE];
    for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) ibuf[i] = 0;

    for (unsigned int p = 0; p < PTRS_PER_INDIRECT; p++) {
        int blk = block_map[Inode::MAX_DIRECT + p];
        for (unsigned int b = 0; b < sizeof(int); b++) {
            ibuf[p * sizeof(int) + b] = (unsigned char)((blk >> (b * 8)) & 0xFF);
        }
    }
    fs->disk->write((unsigned long)inode->indirect_block_no, ibuf);
}

/* ADDED (Option 1): Return the physical disk block number for the given
   logical block index, allocating a new block from the free list if the
   slot is currently empty (-1). Returns -1 if the disk is full. */
int File::get_or_alloc_block(unsigned int logical_block_idx) {
    if (logical_block_idx >= MAX_FILE_BLOCKS) return -1;
    if (block_map[logical_block_idx] >= 0) {
        return block_map[logical_block_idx]; /* already allocated */
    }
    /* Allocate a new disk block */
    int blk = fs->get_free_block();
    if (blk < 0) return -1; /* disk full */
    block_map[logical_block_idx] = blk;
    num_allocated_blocks++;
    return blk;
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char *_buf) {
    Console::puts("reading from file\n");

    /* ADDED: Nothing to return if we are at or past end-of-file */
    if (inode == nullptr || current_position >= inode->file_size) {
        return 0;
    }

    /* ADDED: Clamp read so we never go past the recorded file length */
    unsigned int bytes_left = inode->file_size - current_position;
    unsigned int to_read    = (_n < bytes_left) ? _n : bytes_left;

    /* ADDED (Option 1): Copy bytes from the multi-block cache.
       The cache is a flat array: byte at logical offset O lives at
       multi_block_cache[O], so we can just memcpy. */
    for (unsigned int i = 0; i < to_read; i++) {
        _buf[i] = (char)multi_block_cache[current_position + i];
    }
    current_position += to_read;

    return (int)to_read;
}

int File::Write(unsigned int _n, const char *_buf) {
    Console::puts("writing to file\n");

    if (inode == nullptr) {
        return 0;
    }

    /* ADDED (Option 1): Clamp write to the maximum supported file size */
    unsigned int space_left = MAX_FILE_SIZE - current_position;
    unsigned int to_write   = (_n < space_left) ? _n : space_left;

    /* ADDED (Option 1): Write byte-by-byte into the flat multi-block cache,
       lazily allocating a new disk block whenever we cross a block boundary
       and the next logical block hasn't been assigned yet. */
    unsigned int written = 0;
    while (written < to_write) {
        unsigned int logical_block = (current_position + written) / SimpleDisk::BLOCK_SIZE;

        /* Ensure this logical block has a physical block assigned */
        if (get_or_alloc_block(logical_block) < 0) {
            break; /* disk full — stop here */
        }

        /* Write bytes until the end of this logical block or end of request */
        unsigned int offset_in_block = (current_position + written) % SimpleDisk::BLOCK_SIZE;
        unsigned int space_in_block  = SimpleDisk::BLOCK_SIZE - offset_in_block;
        unsigned int chunk = (to_write - written < space_in_block)
                             ? (to_write - written) : space_in_block;

        for (unsigned int i = 0; i < chunk; i++) {
            multi_block_cache[(current_position + written + i)] = (unsigned char)_buf[written + i];
        }
        written += chunk;
    }

    current_position += written;

    /* ADDED: Extend the logical end-of-file if we wrote past the old boundary */
    if (current_position > inode->file_size) {
        inode->file_size = current_position;
    }

    return (int)written;
}

void File::Reset() {
    Console::puts("resetting file\n");
    /* ADDED: Rewind the read/write cursor to the beginning of the file */
    current_position = 0;
}

bool File::EoF() {
    Console::puts("checking for EoF\n");
    /* ADDED: True when the cursor is at or past the logical end-of-file */
    if (inode == nullptr) return true;
    return current_position >= inode->file_size;
}