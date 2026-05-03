/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
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
#include "file_system.H"

/*--------------------------------------------------------------------------*/
/* CLASS Inode */
/*--------------------------------------------------------------------------*/

/* You may need to add a few functions, for example to help read and store 
   inodes from and to disk. */

/*--------------------------------------------------------------------------*/
/* CLASS FileSystem */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
    Console::puts("In file system constructor.\n");
    /* ADDED: Initialise all pointers to safe defaults.
       The disk is not connected here; that happens in Mount(). */
    disk        = nullptr;
    size        = 0;
    num_blocks  = 0;
    inodes      = nullptr;
    free_blocks = nullptr;
}

FileSystem::~FileSystem() {
    Console::puts("unmounting file system\n");
    /* Make sure that the inode list and the free list are saved. */
    /* ADDED: Flush metadata to disk before releasing memory */
    if (disk != nullptr) {
        save_inodes();
        save_free_blocks();
    }
    if (inodes      != nullptr) { delete[] inodes;      }
    if (free_blocks != nullptr) { delete[] free_blocks; }
}


/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/

/* ADDED: Write the in-memory inode array to INODES_BLOCK (Block 0) on disk.
   The inode structs are serialised as raw bytes into a 512-byte buffer. */
void FileSystem::save_inodes() {
    unsigned char buf[SimpleDisk::BLOCK_SIZE];
    /* Zero-fill so any padding bytes are deterministic */
    for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) buf[i] = 0;

    unsigned int bytes = MAX_INODES * sizeof(Inode);
    if (bytes > SimpleDisk::BLOCK_SIZE) bytes = SimpleDisk::BLOCK_SIZE;
    const unsigned char* src = reinterpret_cast<const unsigned char*>(inodes);
    for (unsigned int i = 0; i < bytes; i++) buf[i] = src[i];

    disk->write(INODES_BLOCK, buf);
}

/* ADDED: Read the inode array from INODES_BLOCK (Block 0) on disk. */
void FileSystem::load_inodes() {
    unsigned char buf[SimpleDisk::BLOCK_SIZE];
    disk->read(INODES_BLOCK, buf);

    unsigned int bytes = MAX_INODES * sizeof(Inode);
    if (bytes > SimpleDisk::BLOCK_SIZE) bytes = SimpleDisk::BLOCK_SIZE;
    unsigned char* dst = reinterpret_cast<unsigned char*>(inodes);
    for (unsigned int i = 0; i < bytes; i++) dst[i] = buf[i];
}

/* ADDED: Write the free-block bitmap to FREELIST_BLOCK (Block 1) on disk.
   We also store num_blocks in the last 4 bytes of the block so Mount()
   can recover the file-system geometry without needing a separate header. */
void FileSystem::save_free_blocks() {
    unsigned char buf[SimpleDisk::BLOCK_SIZE];
    for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) buf[i] = 0;

    /* Copy bitmap (one byte per block) */
    unsigned int bmap_bytes = num_blocks;
    if (bmap_bytes > SimpleDisk::BLOCK_SIZE - sizeof(unsigned int))
        bmap_bytes = SimpleDisk::BLOCK_SIZE - sizeof(unsigned int);
    for (unsigned int i = 0; i < bmap_bytes; i++) buf[i] = free_blocks[i];

    /* Store num_blocks in the last 4 bytes (little-endian) */
    unsigned int offset = SimpleDisk::BLOCK_SIZE - sizeof(unsigned int);
    for (unsigned int b = 0; b < sizeof(unsigned int); b++) {
        buf[offset + b] = (unsigned char)((num_blocks >> (b * 8)) & 0xFF);
    }
    disk->write(FREELIST_BLOCK, buf);
}

/* ADDED: Read the free-block bitmap from FREELIST_BLOCK (Block 1).
   Also recovers num_blocks from the last 4 bytes written by save_free_blocks(). */
void FileSystem::load_free_blocks() {
    unsigned char buf[SimpleDisk::BLOCK_SIZE];
    disk->read(FREELIST_BLOCK, buf);

    /* Recover num_blocks from the last 4 bytes */
    unsigned int offset = SimpleDisk::BLOCK_SIZE - sizeof(unsigned int);
    unsigned int n = 0;
    for (unsigned int b = 0; b < sizeof(unsigned int); b++) {
        n |= ((unsigned int)buf[offset + b]) << (b * 8);
    }
    num_blocks = n;

    /* Re-allocate free_blocks array now that we know the size */
    if (free_blocks != nullptr) delete[] free_blocks;
    free_blocks = new unsigned char[num_blocks];

    unsigned int bmap_bytes = num_blocks;
    if (bmap_bytes > SimpleDisk::BLOCK_SIZE - sizeof(unsigned int))
        bmap_bytes = SimpleDisk::BLOCK_SIZE - sizeof(unsigned int);
    for (unsigned int i = 0; i < bmap_bytes; i++) free_blocks[i] = buf[i];
}

/* ADDED: Find the first free data block, mark it used, flush the bitmap,
   and return its block index. Returns -1 if the disk is full. */
int FileSystem::get_free_block() {
    for (unsigned int i = 0; i < num_blocks; i++) {
        if (free_blocks[i] == 0) {
            free_blocks[i] = 1;
            save_free_blocks();
            return (int)i;
        }
    }
    return -1;
}

/* ADDED: Find the first inode slot whose id == -1 (unused) and return
   its index, or -1 if the table is full. */
int FileSystem::get_free_inode() {
    for (unsigned int i = 0; i < MAX_INODES; i++) {
        if (inodes[i].id == -1L) return (int)i;
    }
    return -1;
}

bool FileSystem::Mount(SimpleDisk * _disk) {
    Console::puts("mounting file system from disk\n");
    /* Here you read the inode list and the free list into memory */

    /* ADDED: Attach disk */
    disk = _disk;

    /* ADDED: Allocate the inode table (size discovered via load_free_blocks) */
    inodes = new Inode[MAX_INODES];

    /* ADDED: load_free_blocks() recovers num_blocks and allocates free_blocks */
    load_free_blocks();

    /* ADDED: Now load the inode table */
    load_inodes();

    /* ADDED: Restore the fs back-pointer in every inode (not serialised) */
    for (unsigned int i = 0; i < MAX_INODES; i++) {
        inodes[i].fs = this;
    }

    return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) { // static!
    Console::puts("formatting disk\n");
    /* Here you populate the disk with an initialized (probably empty) inode list
       and a free list. Make sure that blocks used for the inodes and for the free list
       are marked as used, otherwise they may get overwritten. */

    /* ADDED: Derive geometry */
    unsigned int n_blocks = _size / SimpleDisk::BLOCK_SIZE;

    /* ADDED: Build and write the INODES block (Block 0).
       Fill with zeroed-out Inode structs that all have id == -1 (unused).
       MODIFIED (Option 1): blank inode now initialises block_no[] array and
       indirect_block_no to -1. */
    unsigned char inode_buf[SimpleDisk::BLOCK_SIZE];
    for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) inode_buf[i] = 0;

    Inode blank;
    blank.id              = -1L;
    blank.file_size       = 0;
    blank.fs              = nullptr;
    blank.indirect_block_no = -1;
    for (unsigned int d = 0; d < Inode::MAX_DIRECT; d++) {
        blank.block_no[d] = -1;
    }

    unsigned int max_inodes = SimpleDisk::BLOCK_SIZE / sizeof(Inode);
    const unsigned char* raw = reinterpret_cast<const unsigned char*>(&blank);
    for (unsigned int i = 0; i < max_inodes; i++) {
        unsigned int off = i * sizeof(Inode);
        if (off + sizeof(Inode) > SimpleDisk::BLOCK_SIZE) break;
        for (unsigned int b = 0; b < sizeof(Inode); b++) {
            inode_buf[off + b] = raw[b];
        }
    }
    _disk->write(INODES_BLOCK, inode_buf);

    /* ADDED: Build and write the FREELIST block (Block 1).
       Blocks 0 (INODES) and 1 (FREELIST) are pre-marked used (value = 1).
       num_blocks is stored in the last 4 bytes of this block. */
    unsigned char fl_buf[SimpleDisk::BLOCK_SIZE];
    for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) fl_buf[i] = 0;

    unsigned int bmap_bytes = n_blocks;
    if (bmap_bytes > SimpleDisk::BLOCK_SIZE - sizeof(unsigned int))
        bmap_bytes = SimpleDisk::BLOCK_SIZE - sizeof(unsigned int);
    for (unsigned int i = 0; i < bmap_bytes; i++) {
        fl_buf[i] = (i < 2) ? 1 : 0; /* blocks 0 and 1 are reserved */
    }
    /* Store n_blocks in the last 4 bytes */
    unsigned int offset = SimpleDisk::BLOCK_SIZE - sizeof(unsigned int);
    for (unsigned int b = 0; b < sizeof(unsigned int); b++) {
        fl_buf[offset + b] = (unsigned char)((n_blocks >> (b * 8)) & 0xFF);
    }
    _disk->write(FREELIST_BLOCK, fl_buf);

    return true;
}

Inode * FileSystem::LookupFile(int _file_id) {
    Console::puts("looking up file with id = "); Console::puti(_file_id); Console::puts("\n");
    /* Here you go through the inode list to find the file. */

    /* ADDED: Linear scan through the inode table */
    for (unsigned int i = 0; i < MAX_INODES; i++) {
        if (inodes[i].id == (long)_file_id) {
            return &inodes[i];
        }
    }
    return nullptr;
}

bool FileSystem::CreateFile(int _file_id) {
    Console::puts("creating file with id:"); Console::puti(_file_id); Console::puts("\n");
    /* Here you check if the file exists already. If so, throw an error.
       Then get yourself a free inode and initialize all the data needed for the
       new file. After this function there will be a new file on disk. */

    /* ADDED: Reject if file already exists */
    if (LookupFile(_file_id) != nullptr) {
        Console::puts("CreateFile: file already exists\n");
        return false;
    }

    /* ADDED: Find a free inode slot */
    int inode_idx = get_free_inode();
    if (inode_idx < 0) {
        Console::puts("CreateFile: no free inodes\n");
        return false;
    }

    /* ADDED: Populate the inode.
       MODIFIED (Option 1): initialise block_no[] array and indirect_block_no.
       We do NOT pre-allocate any data blocks here; blocks are allocated lazily
       on the first Write() that needs them. */
    inodes[inode_idx].id              = (long)_file_id;
    inodes[inode_idx].file_size       = 0;
    inodes[inode_idx].fs              = this;
    inodes[inode_idx].indirect_block_no = -1;
    for (unsigned int d = 0; d < Inode::MAX_DIRECT; d++) {
        inodes[inode_idx].block_no[d] = -1;
    }

    /* ADDED: Persist updated inode list */
    save_inodes();

    return true;
}

bool FileSystem::DeleteFile(int _file_id) {
    Console::puts("deleting file with id:"); Console::puti(_file_id); Console::puts("\n");
    /* First, check if the file exists. If not, throw an error. 
       Then free all blocks that belong to the file and delete/invalidate 
       (depending on your implementation of the inode list) the inode. */

    /* ADDED: Locate the inode */
    Inode* node = LookupFile(_file_id);
    if (node == nullptr) {
        Console::puts("DeleteFile: file not found\n");
        return false;
    }

    /* ADDED (Option 1): Free every direct data block */
    for (unsigned int d = 0; d < Inode::MAX_DIRECT; d++) {
        if (node->block_no[d] >= 0 && (unsigned int)node->block_no[d] < num_blocks) {
            free_blocks[node->block_no[d]] = 0;
        }
    }

    /* ADDED (Option 1): Free all blocks listed in the indirect block, then
       free the indirect block itself. */
    if (node->indirect_block_no >= 0 &&
        (unsigned int)node->indirect_block_no < num_blocks) {

        /* Read the indirect block to get the list of data-block numbers */
        unsigned char ibuf[SimpleDisk::BLOCK_SIZE];
        disk->read((unsigned long)node->indirect_block_no, ibuf);

        unsigned int ptrs = SimpleDisk::BLOCK_SIZE / sizeof(int);
        for (unsigned int p = 0; p < ptrs; p++) {
            int blk = 0;
            for (unsigned int b = 0; b < sizeof(int); b++) {
                blk |= ((int)ibuf[p * sizeof(int) + b]) << (b * 8);
            }
            if (blk >= 0 && (unsigned int)blk < num_blocks) {
                free_blocks[blk] = 0;
            }
        }
        /* Free the indirect block itself */
        free_blocks[node->indirect_block_no] = 0;
    }

    save_free_blocks();

    /* ADDED: Invalidate the inode */
    node->id              = -1L;
    node->file_size       = 0;
    node->indirect_block_no = -1;
    for (unsigned int d = 0; d < Inode::MAX_DIRECT; d++) {
        node->block_no[d] = -1;
    }

    /* ADDED: Persist updated inode list */
    save_inodes();

    return true;
}