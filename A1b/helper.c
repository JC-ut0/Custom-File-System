#include "helper.h"


/** Helper to calculate the number of blocks needed by total_size in block_unit. */
int calculate_blocks_needed(int total_size, int block_unit){
	int result = total_size / block_unit;
	if(total_size % block_unit){
		result++;
	}
	return result;
}

/** set bit_map at index **/
void set_bitmap(char *bit_map, int index){
	char *byte = bit_map + (index / 8);
	*byte |= 1 << (index % 8);
}

/** free bit_map at index **/
void free_bitmap(char *bit_map, int index){
	char *byte = bit_map + (index / 8);
	*byte = *byte & ~(1 << (index % 8));
}

/** read bit_map at index **/
bool read_bitmap(char *bit_map, int index){
	char *byte = bit_map + (index / 8);
	return 1 && (*byte & (1 << (index % 8)));
}

/** find first free inode by given superblock sp **/
int find_first_free_inode_num(struct a1fs_superblock *sp){
	int inuse;
	unsigned int byte;
	char *inode_bits = (char *)((void*)sp + A1FS_BLOCK_SIZE * sp->inode_bitmap);
	for(byte = 0; byte <  sp->max_inodes_count/8; byte++ ){
		for(int bit = 0; bit < 8; bit++){
			inuse = 1 && (inode_bits[byte] & (1 << bit));
			if (! inuse){
				return 8 * byte + bit;
			}
		}
	}
	if(sp->max_inodes_count/8 == 0){
		for(unsigned int bit = 0; bit < sp->max_inodes_count%8; bit++){
			inuse = 1 && (inode_bits[byte] & (1 << bit));
			if (! inuse){
				return 8 * byte + bit;
			}
		}
	}
	
	return -1;
}

/** find first free block by given superblock sp **/
int find_first_free_block_num(struct a1fs_superblock *sp){
	int inuse;
	unsigned int byte;
	char *block_bitmap = (char *)((void*)sp + A1FS_BLOCK_SIZE * sp->block_bitmap);
	for( byte = 0; byte <  sp->max_block_count/8; byte++ ){
		for(int bit = 0; bit < 8; bit++){
			inuse = 1 && (block_bitmap[byte] & (1 << bit));
			if (! inuse){
				return 8 * byte + bit;
			}
		}
	}
	if(sp->max_block_count/8 == 0){
		for(unsigned int bit = 0; bit < sp->max_block_count%8; bit++){
				inuse = 1 && (block_bitmap[byte] & (1 << bit));
				if (! inuse){
					return 8 * byte + bit;
				}
			}
	}
	return -1;
}

/* whether inode replaceable */
bool replaceable(a1fs_inode *inode){
	if (S_ISDIR(inode->mode)){
		// only replaceable when the dirctory is empty
		return (inode->size) == 0;
	}
	// always replaceable if is file
	return true;
}

/* update new_inode with old_inode */
void update_inode(a1fs_inode *new_inode, a1fs_inode *old_inode){
	new_inode->links = old_inode->links;
	new_inode->mode = old_inode->mode;
	new_inode->size = old_inode->size;
	new_inode->blocks = old_inode->blocks;
	clock_gettime(CLOCK_REALTIME, &new_inode->mtime);
}


/* increment inode size */
void increment_size(a1fs_inode *inode, int size){
	clock_gettime(CLOCK_REALTIME, &inode->mtime);
	inode->size += size;
}



