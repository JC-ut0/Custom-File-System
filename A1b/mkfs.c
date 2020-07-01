/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 Karen Reid
 */
 
/**
 * CSC369 Assignment 1 - a1fs formatting tool.
 */
 
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
 
#include "a1fs.h"
#include "map.h"
#include "helper.h"
 
 
/** Command line options. */
typedef struct mkfs_opts {
    /** File system image file path. */
    const char *img_path;
    /** Number of inodes. */
    size_t n_inodes;
 
    /** Print help and exit. */
    bool help;
    /** Overwrite existing file system. */
    bool force;
    /** Sync memory-mapped image file contents to disk. */
    bool sync;
    /** Verbose output. If false, the program must only print errors. */
    bool verbose;
    /** Zero out image contents. */
    bool zero;
 
} mkfs_opts;
 
static const char *help_str = "\
Usage: %s options image\n\
\n\
Format the image file into a1fs file system. The file must exist and\n\
its size must be a multiple of a1fs block size - %zu bytes.\n\
\n\
Options:\n\
    -i num  number of inodes; required argument\n\
    -h      print help and exit\n\
    -f      force format - overwrite existing a1fs file system\n\
    -s      sync image file contents to disk\n\
    -v      verbose output\n\
    -z      zero out image contents\n\
";
 
static void print_help(FILE *f, const char *progname)
{
    fprintf(f, help_str, progname, A1FS_BLOCK_SIZE);
}
 
 
static bool parse_args(int argc, char *argv[], mkfs_opts *opts)
{
    char o;
    while ((o = getopt(argc, argv, "i:hfsvz")) != -1) {
        switch (o) {
            case 'i': opts->n_inodes = strtoul(optarg, NULL, 10); break;
 
            case 'h': opts->help    = true; return true;// skip other arguments
            case 'f': opts->force   = true; break;
            case 's': opts->sync    = true; break;
            case 'v': opts->verbose = true; break;
            case 'z': opts->zero    = true; break;
 
            case '?': return false;
            default : assert(false);
        }
    }
 
    if (optind >= argc) {
        fprintf(stderr, "Missing image path\n");
        return false;
    }
    opts->img_path = argv[optind];
 
    if (opts->n_inodes == 0) {
        fprintf(stderr, "Missing or invalid number of inodes\n");
        return false;
    }
    return true;
}
 
 
/** Determine if the image has already been formatted into a1fs. */
static bool a1fs_is_present(void *image)
{
    struct a1fs_superblock *sp   = (struct a1fs_superblock*)image;
    return sp->magic == A1FS_MAGIC && sp->state == 1;
}
 
 
/**
 * Format the image into a1fs.
 *
 * NOTE: Must update mtime of the root directory.
 *
 * @param image  pointer to the start of the image.
 * @param size   image size in bytes.
 * @param opts   command line options.
 * @return       true on success;
 *               false on error, e.g. options are invalid for given image size.
 */
static bool mkfs(void *image, size_t size, mkfs_opts *opts) {
   //TODO: initialize the superblock and create an empty root directory
   struct a1fs_superblock *sp   = (struct a1fs_superblock*)image;
   sp->magic = A1FS_MAGIC;
   sp->size = size;
   sp->max_inodes_count = opts->n_inodes;
  
   // Block number of inode bitmap
   sp->inode_bitmap = 1;
   unsigned int num_blocks_for_inode_bitmap = calculate_blocks_needed(sp->max_inodes_count, A1FS_BLOCK_SIZE * 8);
   if(num_blocks_for_inode_bitmap > sp->max_block_count - 1) return false; // check wethter exceed the max blocks
 
   // set all bitmap to 0
   memset((void*)sp + sp->inode_bitmap * A1FS_BLOCK_SIZE, 0, A1FS_BLOCK_SIZE * num_blocks_for_inode_bitmap);
 
   // space needed to include all inode struct
   int inode_total_space = sizeof(a1fs_inode) * sp->max_inodes_count;
   unsigned int num_blocks_for_inode_table =  calculate_blocks_needed(inode_total_space, A1FS_BLOCK_SIZE);
   if(num_blocks_for_inode_table > sp->max_block_count - 1 - num_blocks_for_inode_bitmap) return false;
 
   // Block number of block bitmap
   sp->block_bitmap = 1 + num_blocks_for_inode_bitmap;
   sp->max_block_count = size / A1FS_BLOCK_SIZE;
   unsigned int num_blocks_for_block_bitmap = calculate_blocks_needed(sp->max_block_count, A1FS_BLOCK_SIZE * 8);
   if(num_blocks_for_block_bitmap > sp->max_block_count - 1 - num_blocks_for_inode_bitmap) return false;
   
   // set all bitmap to 0
   memset((void*)sp + sp->block_bitmap * A1FS_BLOCK_SIZE, 0, A1FS_BLOCK_SIZE * num_blocks_for_block_bitmap);
 
   // Block number of inode table
   sp->inode_table = sp->block_bitmap + num_blocks_for_block_bitmap;
 
   sp->inode_size = sizeof(struct a1fs_inode);
 
   // create empty root directory (only inode is needed)
   // set inode bitmap 0 to 1
   char *inode_bitmap = (char *)((void*)sp + A1FS_BLOCK_SIZE * sp->inode_bitmap);
   set_bitmap(inode_bitmap, 0);// for error handle
   set_bitmap(inode_bitmap, 1); //for root
  
   struct a1fs_inode *iroot = (struct a1fs_inode*)((void*)sp + A1FS_BLOCK_SIZE * sp->inode_table + 1 * sizeof(a1fs_inode));
 
   iroot->mode = S_IFDIR;
   iroot->links = 2; // one for '.', one for '..'
   iroot->size = 0; // the size of the dentry
   clock_gettime(CLOCK_REALTIME, &iroot->mtime);
   iroot->blocks = 0;
 
   sp->inodes_count = 2;
   sp->blocks_count = 1 + num_blocks_for_block_bitmap + num_blocks_for_inode_bitmap + num_blocks_for_inode_table;
   sp->free_blocks_count = sp->max_block_count - sp->blocks_count;
   sp->free_inodes_count = sp->max_inodes_count - sp->inodes_count;
   
   // set block bitmap of all reserved block to 1
   char *block_bits = (char *)((void*)sp + A1FS_BLOCK_SIZE * sp->block_bitmap);
   for (size_t i = 0; i < sp->blocks_count; i++){
       set_bitmap(block_bits, i);
   }

   if (sp->max_block_count < sp->blocks_count || sp->max_inodes_count < sp->inodes_count){
       sp->state = 2;// 1 for valid; 2 for error
       return false;
   }
   sp->state = 1;
   return true;
}
 
 
 
 
int main(int argc, char *argv[])
{
    mkfs_opts opts = {0};// defaults are all 0
    if (!parse_args(argc, argv, &opts)) {
        // Invalid arguments, print help to stderr
        print_help(stderr, argv[0]);
        return 1;
    }
    if (opts.help) {
        // Help requested, print it to stdout
        print_help(stdout, argv[0]);
        return 0;
    }
 
    // Map image file into memory
    size_t size;
    void *image = map_file(opts.img_path, A1FS_BLOCK_SIZE, &size);
    if (image == NULL) return 1;
 
    // Check if overwriting existing file system
    int ret = 1;
    if (!opts.force && a1fs_is_present(image)) {
        fprintf(stderr, "Image already contains a1fs; use -f to overwrite\n");
        goto end;
    }
 
    if (opts.zero) memset(image, 0, size);
    if (!mkfs(image, size, &opts)) {
        fprintf(stderr, "Failed to format the image\n");
        goto end;
    }
 
    // Sync to disk if requested
    if (opts.sync && (msync(image, size, MS_SYNC) < 0)) {
        perror("msync");
        goto end;
    }
 
    ret = 0;
end:
    munmap(image, size);
    return ret;
}
 
 

