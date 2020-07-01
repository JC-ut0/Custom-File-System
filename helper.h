#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "a1fs.h"
#include "map.h"


/** Helper to calculate the number of blocks needed by total_size in block_unit. */
int calculate_blocks_needed(int total_size, int block_unit);

/** set bit_map at index **/
void set_bitmap(char *bit_map, int index);

/** free bit_map at index **/
void free_bitmap(char *bit_map, int index);

/** read bit_map at index **/
bool read_bitmap(char *bit_map, int index);

/** 
 * find first free inode by given superblock sp
 * return -1 on error
 */
int find_first_free_inode_num(struct a1fs_superblock *sp);

/** 
 * find first free block by given superblock sp
 * return -1 on error
 */
int find_first_free_block_num(struct a1fs_superblock *sp);

/* whether inode replaceable */
bool replaceable(a1fs_inode *inode);

/* update new_inode with old_inode */
void update_inode(a1fs_inode *new_inode, a1fs_inode *old_inode);

/* increment inode size */
void increment_size(a1fs_inode *inode, int size);
