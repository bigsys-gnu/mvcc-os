#pragma once

// On-disk file system format. 
// Both the kernel and user programs use this header file.

#include <uk/fs.h>

// Block 0 is unused.
// Block 1 is super block.
// Inodes start at block 2.

#define ROOTINO 1  // root i-number
#define BSIZE 4096  // block size
// total number of disk blocks in fs.img, see tools/mkfs.c
#define BLKS_PER_MEG 256

#if defined(HW_qemu)
#define NMEGS (NCPU) * 128 // 1 GB
#else
#define NMEGS 16384 // 16 GB
#endif


// File system super block
// assert(sizeof(superblock) <= BSIZE)

struct superblock {
  u32 size;         // Size of file system image (blocks)
  u32 nblocks;      // Number of data blocks
  u32 ninodes;      // Number of inodes.
  struct journal_blknums {
    u32 start_blknum;
    u32 end_blknum; // Inclusive
  } journal_blknums[NCPU];
};


#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(u32))
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT*NINDIRECT)

// Size of the physical journal file - /sv6journal
#define PHYS_JOURNAL_SIZE ((NDIRECT + NINDIRECT) * BSIZE)

// Considerations in determining the value of PHYS_JOURNAL_SIZE:
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// A simple example for a large transaction would be unlinking (or truncating)
// a large file: it involves logging all the updated metadata blocks of the
// file. The largest file that the filesystem can support has upto 1026 metadata
// blocks:
//
// Indirect blocks: 1
// Doubly-indirect blocks: 1 + 1024 (BSIZE/sizeof(u32) i.e. 4096/4)
//
// So a transaction logging the unlink of this file would look like this:
//
// [ Size of header = 16 (i.e., sizeof(journal_block_header)) ]
//
// Start header : 16 + 4096 (BSIZE)
//
// Transaction data blocks:
// Every datablock goes with a header, so: Num-data-blocks * (16 + BSIZE)
//
// Along with the file metadata, we will also need to log changes to the
// free bitmap blocks. This could span NUM_FS_BLOCKS / (BSIZE * 8) blocks, which
// fits in 1 diskblock for a filesystem of size 128 MB.
//
// So Num-data-blocks = 1026 + 1 = 1027.
//
// Commit header: 16 + 4096 (BSIZE)
//
// So in total, a transaction updating 1027 blocks will consume upto 1034 blocks
// in the journal, as shown below:
// 4112 + 1027 * (16 + 4096) + 4112 = 4231248 bytes (~ 1034 blocks)
//
// So if you are about to surpass transactions of this size, remember to enlarge
// the physical journal!


// On-disk inode structure
// (BSIZE % sizeof(dinode)) == 0
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  u32 size;             // Size of file (bytes)
  u32 gen;              // Generation # (to check name cache)
  u32 addrs[NDIRECT+2]; // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) ((b)/BPB + (ninodes)/IPB + 3)

// Number of inodes to create in the filesystem. Consumed by tools/mkfs.c
// as well as kernel/scalefs.cc (to decide the size of the inum<->mnode
// lookup tables). If you change this number, remember to update NINODES_PRIME
// in kernel/scalefs.cc
#if defined(HW_qemu)
#define NINODES		4000
#else
#define NINODES		1000000
#endif

// A prime number larger than NINODES
#if defined(HW_qemu)
#define NINODES_PRIME	4013
#else
#define NINODES_PRIME	1010003
#endif

// A prime number larger than the total number of inode and bitmap blocks.
// (They are currently around 15000 for a 16GB filesystem).
#if defined(HW_qemu)
#define NINODEBITMAP_BLKS_PRIME	311
#else
#define NINODEBITMAP_BLKS_PRIME	30011
#endif

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  u16 inum;
  char name[DIRSIZ];
};

// XXX(Austin) PATH_MAX sucks.  It would be nice if we didn't need it
// to size kernel copy buffers.
#define PATH_MAX 256
