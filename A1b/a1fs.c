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
 * CSC369 Assignment 1 - a1fs driver implementation.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <math.h>

// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "a1fs.h"
#include "fs_ctx.h"
#include "options.h"
#include "map.h"
#include "helper.h"

//NOTE: All path arguments are absolute paths within the a1fs file system and
// start with a '/' that corresponds to the a1fs root directory.
//
// For example, if a1fs is mounted at "~/my_csc369_repo/a1b/mnt/", the path to a
// file at "~/my_csc369_repo/a1b/mnt/dir/file" (as seen by the OS) will be
// passed to FUSE callbacks as "/dir/file".
//
// Paths to directories (except for the root directory - "/") do not end in a
// trailing '/'. For example, "~/my_csc369_repo/a1b/mnt/dir/" will be passed to
// FUSE callbacks as "/dir".


/**
 * Initialize the file system.
 *
 * Called when the file system is mounted. NOTE: we are not using the FUSE
 * init() callback since it doesn't support returning errors. This function must
 * be called explicitly before fuse_main().
 *
 * @param fs    file system context to initialize.
 * @param opts  command line options.
 * @return      true on success; false on failure.
 */
static bool a1fs_init(fs_ctx *fs, a1fs_opts *opts)
{
	// Nothing to initialize if only printing help or version
	if (opts->help || opts->version) return true;

	size_t size;
	void *image = map_file(opts->img_path, A1FS_BLOCK_SIZE, &size);
	if (!image) return false;

	return fs_ctx_init(fs, image, size, opts);
}

/**
 * Cleanup the file system.
 *
 * Called when the file system is unmounted. Must cleanup all the resources
 * created in a1fs_init().
 */
static void a1fs_destroy(void *ctx)
{
	fs_ctx *fs = (fs_ctx*)ctx;
	if (fs->image) {
		if (fs->opts->sync && (msync(fs->image, fs->size, MS_SYNC) < 0)) {
			perror("msync");
		}
		munmap(fs->image, fs->size);
		fs_ctx_destroy(fs);
	}
}

/** Get file system context. */
static fs_ctx *get_fs(void)
{
	return (fs_ctx*)fuse_get_context()->private_data;
}


/**
 * Get file system statistics.
 *
 * Implements the statvfs() system call. See "man 2 statvfs" for details.
 * The f_bfree and f_bavail fields should be set to the same value.
 * The f_ffree and f_favail fields should be set to the same value.
 * The following fields can be ignored: f_fsid, f_flag.
 * All remaining fields are required.
 *
 * @param path  path to any file in the file system. Can be ignored.
 * @param st    pointer to the struct statvfs that receives the result.
 * @return      0 on success; -errno on error.
 */
static int a1fs_statfs(const char *path, struct statvfs *st)
{
	(void)path;// unused
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));
	st->f_bsize   = A1FS_BLOCK_SIZE;
	st->f_frsize  = A1FS_BLOCK_SIZE;
	//TODO: fill in the rest of required fields based on the information stored
	// in the superblock
	st->f_blocks = fs->size / st->f_frsize;

	struct a1fs_superblock *sp = (struct a1fs_superblock*)fs->image;

	st->f_bfree	 = 	sp->free_blocks_count;
	st->f_bavail = 	sp->free_blocks_count;
	st->f_files	 = 	sp->inodes_count;
	st->f_ffree	 = 	sp->free_inodes_count;
	st->f_favail = 	sp->free_inodes_count;

	st->f_namemax = A1FS_NAME_MAX;
	
	return 0;
}


/**
 * Given the inode for a directory
 * Given the name of the dentry that we are looking for
 *
 * Return the order of that file/directory in its parent data
 * Update pos to be the pointer pointing to dentry
 * Return -1 if the dentry of that file/directory is not found
 */
int find_dentry_from_inode(a1fs_inode* p_inode, a1fs_dentry** result_dentry, char* name) {
    fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    int num_dentries_searched = 0;
    int total_num_dentries = (int) (p_inode->size / sizeof(a1fs_dentry));
    a1fs_extent *extent = (a1fs_extent*)((void*)sp + A1FS_BLOCK_SIZE * (p_inode->extents).start);
    a1fs_dentry *dentry = (a1fs_dentry*) ((void*)sp + extent->start * A1FS_BLOCK_SIZE);
    while (num_dentries_searched < total_num_dentries) {
        // iterate through the extents of p_inode to find the dentry
        if (strcmp(name, dentry->name) == 0) {
            *result_dentry = dentry;
            return num_dentries_searched;
            }
        dentry += 1;
        if ((a1fs_dentry*) ((void*)sp + A1FS_BLOCK_SIZE * (extent->start + extent->count)) == dentry) {
            // the current extent is already searched completely
            // Forward to the next extent
            extent += 1;
        }
        num_dentries_searched += 1;
    }
    return -1;
}


/**
 * Get the inode of the file indicated by the givenpath
 * If success, result should be the pointer to the inode of the file on return
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * @param path the path to the specific file
 * @param result the pointer to the inode of the file that needs to be updated
 * @return 0 on success; -errnor on errors;
 *
 */
int find_inode_from_path(const char* path, a1fs_inode** result) {
    if (strlen(path) >= A1FS_PATH_MAX) return -ENAMETOOLONG;
    fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    char path_copy[A1FS_PATH_MAX];
	strncpy(path_copy, path, A1FS_PATH_MAX);
	path_copy[A1FS_PATH_MAX-1] = '\0';
	char *name = strtok(path_copy, "/");
    a1fs_dentry *curr_dentry;
    int error_result;
    // starting from root
    a1fs_inode *curr_inode = (a1fs_inode*)((void*)sp + A1FS_BLOCK_SIZE * sp->inode_table + sizeof(a1fs_inode));
    while (name) {
        // after each iteraton, curr_dentry and curr_inode should be corresponding to the same file
        if (strlen(name) >= A1FS_NAME_MAX) return -ENAMETOOLONG;
        if (!S_ISDIR(curr_inode->mode)) return -ENOTDIR;
        error_result = find_dentry_from_inode(curr_inode, &curr_dentry, name);
        if (error_result < 0) return -ENOENT;
        curr_inode = (a1fs_inode*) ((void*)sp + sp->inode_table * A1FS_BLOCK_SIZE + curr_dentry->ino * sizeof(a1fs_inode));
        name = strtok(NULL, "/");
    }
    // Now curr_dir should be the file that we are looking for
    *result = curr_inode;
    return 0;
}


/**
 *  Find the pointer pointing to the file offset from the beginning by size
 *  On return:
 *  ptr should be pointing to the byte right after the offset
 *  extent should be pointing to the extent where the actual last bit of the file is in
 *
 * Assumption:
 *      file is not empty
 *      0 <= size <= file size
 *
 *  @param inode the inode that we are interested in
 */
void find_ptr_at_size(a1fs_inode *inode, size_t size, char **ptr, a1fs_extent **last_extent) {
    fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    int byte_searched = 0;
    a1fs_extent *extent = (a1fs_extent*) ((void*)sp + inode->extents.start * A1FS_BLOCK_SIZE);
    char * curr_ptr = (char*) ((void*)sp + extent->start * A1FS_BLOCK_SIZE);
    while (byte_searched < (int) size) {
        if ((char*) ((void*)sp + A1FS_BLOCK_SIZE * (extent->start + extent->count)) == curr_ptr) {
            // the current extent is already searched completely
            // Forward to the next extent
            extent += 1;
            curr_ptr = (char*) ((void*)sp + extent->start * A1FS_BLOCK_SIZE);
        }
        byte_searched += 1;
        curr_ptr += 1;
    }
    *ptr = curr_ptr;
    *last_extent = extent;
}


/**
 * initialize a new extent at required pos
 *
 * @param pos where we want to add the extent
 * @Return the pointer to the first bit of the data block of the new extent
 */
void* add_extent(a1fs_extent* pos) {
    fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    a1fs_blk_t new_extent_start = find_first_free_block_num(sp);
    char *block_bitmap = (char*) ((void*)sp + sp->block_bitmap * A1FS_BLOCK_SIZE);
    set_bitmap(block_bitmap, new_extent_start);
    sp->free_blocks_count -= 1;
    sp->blocks_count += 1;
    pos->start = new_extent_start;
    pos->count = 1;
    void* result = (void*) ((void*)sp + new_extent_start * A1FS_BLOCK_SIZE);
    return result;
}


/**
 * Allocate data space of required size for p_inode
 * pos pointed to the first byte of the new allocated space on return
 * pos should be only used on success return
 *
 * Errors:
 *  ENOSPC not enough free space in the file system
 *
 *  @param p_inode the pointer to the inode of the parent
 *  @param pos pointer pointing to the new allocated space
 *  @param size the size of the new allocated space
 */
int add_data(a1fs_inode *p_inode, char** pos, size_t size) {
    fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    char *curr_ptr;
    a1fs_extent* extent;
    if (p_inode->size == 0) {
        // We need to initialize the extents(indirect) of p_inode and one direct extent
        if (sp->free_blocks_count < 2) return -ENOSPC;
        extent = (a1fs_extent*) add_extent(&(p_inode->extents));
        curr_ptr = (char*) add_extent(extent);
        p_inode->blocks += 2;
    } else find_ptr_at_size(p_inode, p_inode->size, &curr_ptr, &extent);
    *pos = curr_ptr;
    // Now increase length of extents or allocate new extents if needed
    uint64_t byte_searched = 0;
    while (byte_searched < size) {
        if ((char*) ((void*)sp + A1FS_BLOCK_SIZE * (extent->start + extent->count)) == curr_ptr) {
            // the current extent is already searched completely and we need to use more blocks
            // check if we have enough space
            if (sp->blocks_count >= sp->max_block_count) return -ENOSPC;
            char *block_bitmap = (char*) ((void*)sp + sp->block_bitmap * A1FS_BLOCK_SIZE);
            int index = extent->start + extent->count;
            if (!read_bitmap(block_bitmap, index)) {
                // extent the extent if we can
                set_bitmap(block_bitmap, index);
                sp->free_blocks_count -= 1;
                sp->blocks_count += 1;
                extent->count += 1;
            } else {
                // else we need to allocate a new extent
                // check if we have already allocated 512 extents for this file
                if ((int)(((void*)extent - (void*)sp - (p_inode->extents).start * A1FS_BLOCK_SIZE) / sizeof(a1fs_extent)) >= 512) return -ENOSPC;
                extent += 1;
                // add one more extent after the previous extent position
                curr_ptr = (char*) add_extent(extent);
            }
            p_inode->blocks += 1;
        }
        *curr_ptr = '\0';
        byte_searched += 1;
        curr_ptr += 1;
    }
    p_inode->size += size;
    clock_gettime(CLOCK_REALTIME, &p_inode->mtime);
    return 0;
}


/**
 * Create a new inode for the file specified by dentry
 * Update file_inode to be the created new inode
 * Set size, blocks, mtime for the new inode
 * Need to set mode and links for the new inode
 *
 *
 * Errors:
 *   ENOSPC  not enough free space in the file system.
 *
 * @param file_inode the address of the pointer to the new created inode
 * @param dentry the dentry of the file whose inode need to be created
 * @return 0 on success, -errnor on error
 */
int make_inode(a1fs_inode** file_inode, a1fs_dentry* dentry) {
    fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    if (sp->inodes_count >= sp->max_inodes_count) return -ENOSPC;
    // create file_inode
    char *inode_bitmap = (char*) ((void*)sp + sp->inode_bitmap * A1FS_BLOCK_SIZE);
    int ino = find_first_free_inode_num(sp);
    if (ino < 0 ) return -ENOSPC;
    set_bitmap(inode_bitmap, ino);
    sp->inodes_count += 1;
    sp->free_inodes_count -= 1;
    dentry->ino = ino;
    *file_inode = (a1fs_inode*) ((void*)sp + sp->inode_table * A1FS_BLOCK_SIZE + ino * sizeof(a1fs_inode));
    (*file_inode)->size = 0;
    clock_gettime(CLOCK_REALTIME, &(*file_inode)->mtime);
    (*file_inode)->blocks = 0;
    return 0;
}


/**
* Delete data starting from offset i.e., deleted data region is [offset, offset+size]
* offset should be pointeing to somewhere inside a data block
* size should be valid, i.e., 0 <= size <= left space starting from offset
*
* @param inode the inode of the file whose data need to be deleted
* @param offset the offset from begining of the file to the position of the data that need to deleted
* @param size the size of the data region that need to be deleted
*/
void delete_data (a1fs_inode* inode, uint64_t offset, uint64_t size) {
    assert(offset + size <= inode->size);
    fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    // get the to_ptr pointing at offset and from_ptr pointing at offset+size
    char *to_ptr;
    a1fs_extent* to_extent;
    find_ptr_at_size(inode, offset, &to_ptr, &to_extent);
    char *from_ptr;
    a1fs_extent *from_extent;
    find_ptr_at_size(inode, offset+size, &from_ptr, &from_extent);
    uint64_t byte_searched = 0;
    // Now copy data from from_ptr to to_ptr
    // In total <filesize - (offset + size)> bytes data should be copied
    // uint64_t cpy_amt_once;
    while (byte_searched < (inode->size - offset - size)) {
         if ((char*) ((void*)sp + A1FS_BLOCK_SIZE * (from_extent->start + from_extent->count)) == from_ptr) {
             from_extent += 1;
             from_ptr = (char*) ((void*)sp + A1FS_BLOCK_SIZE * from_extent->start);
             }
         if ((char*) ((void*)sp + A1FS_BLOCK_SIZE * (to_extent->start + to_extent->count)) == to_ptr) {
             to_extent += 1;
             to_ptr = (char*) ((void*)sp + A1FS_BLOCK_SIZE * to_extent->start);
            }
        *to_ptr = *from_ptr;
        to_ptr += 1;
        from_ptr += 1;
        byte_searched += 1;
    }
    // Now we need to check if we need to deallocate any data bitmap
    // Check if we need to decrease length of the new last extent
    a1fs_extent *new_last_extent = to_extent;
    a1fs_blk_t new_last_extent_count = to_extent->count;
    a1fs_blk_t block_index = (a1fs_blk_t) (((void*)to_ptr - (void*)sp) / A1FS_BLOCK_SIZE);
    if (to_extent->start <= block_index && block_index < to_extent->start + to_extent->count - 1) {
        new_last_extent_count =  block_index + 1 - to_extent->start;
    }
    // we starting from <off_set>, copied <filesize-offset-size> bytes data
    // need to check <size> bytes more data to iterate through the whole file/directory
    // Forward ptr to the starting byte of next block since we are only checking if we need to free bitmap
    byte_searched = (uint64_t) ((void*)sp + (new_last_extent->start + new_last_extent_count) * A1FS_BLOCK_SIZE - (void*)to_ptr);
    to_ptr = (char*) ((void*)sp + (new_last_extent->start + new_last_extent_count) * A1FS_BLOCK_SIZE);
    char *block_bitmap = (char*) ((void*)sp + sp->block_bitmap * A1FS_BLOCK_SIZE);
    while (byte_searched < size) {
        if ((char*) ((void*)sp + A1FS_BLOCK_SIZE * (to_extent->start + to_extent->count)) == to_ptr) {
            to_extent += 1;
            to_ptr = (char*) ((void*)sp + to_extent -> start * A1FS_BLOCK_SIZE);
        }
        block_index = (a1fs_blk_t) (((void*)to_ptr - (void*)sp) / A1FS_BLOCK_SIZE);
        free_bitmap(block_bitmap, block_index);
        inode->blocks -= 1;
        sp->blocks_count -= 1;
        sp->free_blocks_count += 1;
        to_extent = (a1fs_extent*) ((void*)sp + to_extent->start * A1FS_BLOCK_SIZE);
        byte_searched += A1FS_BLOCK_SIZE;
        to_ptr = (char*) ((void*)to_ptr + A1FS_BLOCK_SIZE);
    }
    new_last_extent->count = new_last_extent_count;
    clock_gettime(CLOCK_REALTIME, &(inode->mtime));
    inode->size -= size;
    // if the file after deletion is empty, we should delete its indirect extent
    if (inode->size == 0) {
        inode->blocks = 0;
        free_bitmap(block_bitmap, (inode->extents).start);
        sp->blocks_count -= 1;
        sp->free_blocks_count += 1;
    }
}

/**
 * Given the full path
 * On return:
 * Update the p_path to be the full path to the parent
 * Upate name to be the name of the file
 */
void find_parent_path(const char* path, char *p_path, char *name) {
    strncpy(p_path, path, A1FS_PATH_MAX);
    p_path[A1FS_PATH_MAX-1] = '\0';
    int i;
    for (i = strlen(path); i >= 0; i--) {
        if (p_path[i] == '/') {
            p_path[i] = '\0';
            break;
        }
    }
    strncpy(name, path+i+1, A1FS_NAME_MAX);
    name[A1FS_NAME_MAX-1] = '\0';
}


/**
 * Assumptions: 
 *  The path is valid: all file on the path exist and all intermediate ones are directories
 * path represents a full path to an inode
 * Update the mtime of inode and all of inode's ancestors which are the inode on the given path
 */
void update_time_for_family(const char *path) {
    fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    char path_copy[A1FS_PATH_MAX];
	strncpy(path_copy, path, A1FS_PATH_MAX);
	path_copy[A1FS_PATH_MAX-1] = '\0';
	char *name = strtok(path_copy, "/");
    a1fs_dentry *curr_dentry;
    // starting from root
    a1fs_inode *curr_inode = (a1fs_inode*)((void*)sp + A1FS_BLOCK_SIZE * sp->inode_table + sizeof(a1fs_inode));
    clock_gettime(CLOCK_REALTIME, &(curr_inode->mtime));
    while (name) {
        // after each iteraton, curr_dentry and curr_inode should be corresponding to the same file
        find_dentry_from_inode(curr_inode, &curr_dentry, name);
        curr_inode = (a1fs_inode*) ((void*)sp + sp->inode_table * A1FS_BLOCK_SIZE + curr_dentry->ino * sizeof(a1fs_inode));
        name = strtok(NULL, "/");
        clock_gettime(CLOCK_REALTIME, &(curr_inode->mtime));
    }
}


/**
 * Get file or directory attributes.
 *
 * Implements the stat() system call. See "man 2 stat" for details.
 * The following fields can be ignored: st_dev, st_ino, st_uid, st_gid, st_rdev,
 *                                      st_blksize, st_atim, st_ctim.
 * All remaining fields are required.
 *
 * NOTE: the st_blocks field is measured in 512-byte units (disk sectors).
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * @param path  path to a file or directory.
 * @param st    pointer to the struct stat that receives the result.
 * @return      0 on success; -errno on error;
 */
static int a1fs_getattr(const char *path, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    a1fs_inode* file_inode;
    int result = find_inode_from_path(path, &file_inode);
    if(result != 0) { return result; }
    st->st_nlink = file_inode->links;
    st->st_mode = file_inode->mode | 0777;
    st->st_size = file_inode->size;
    st->st_blocks = file_inode->blocks * A1FS_BLOCK_SIZE / 512;
    st->st_mtim = file_inode->mtime;
    
    // set up for ino can be ignored
    char p_path[A1FS_PATH_MAX];
    char name[A1FS_NAME_MAX];
    find_parent_path(path, p_path, name);
    a1fs_inode *p_inode;
    find_inode_from_path(p_path, &p_inode);
    a1fs_dentry *pos;
    result = find_dentry_from_inode(p_inode, &pos, name);
    if(result < 0){
        st->st_ino = 1;//root
    }else
    {
        int ino = ((a1fs_dentry*)pos)->ino;
        st->st_ino = ino;
    }
    return 0;
}


/**
 * Read a directory.
 *
 * Implements the readdir() system call. Should call filler() for each directory
 * entry. See fuse.h in libfuse source code for details.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a filler() call failed).
 *
 * @param path    path to the directory.
 * @param buf     buffer that receives the result.
 * @param filler  function that needs to be called for each directory entry.
 *                Pass 0 as offset (4th argument). 3rd argument can be NULL.
 * @param offset  unused.
 * @param fi      unused.
 * @return        0 on success; -errno on error.
 */
static int a1fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
    (void)offset;// unused
    (void)fi;// unused
    fs_ctx *fs = get_fs();
    //TODO: lookup the directory inode for given path and iterate through its
    // directory entries
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    // get the file inode
    a1fs_inode* file_inode;
    find_inode_from_path(path, &file_inode);
    int num_dentries_searched = 0;
    int total_num_dentries = file_inode->size / sizeof(a1fs_dentry);
    a1fs_extent *extent = (a1fs_extent*) ((void*)sp + (file_inode->extents).start * A1FS_BLOCK_SIZE);
    a1fs_dentry *dentry = (a1fs_dentry*) ((void*)sp + extent->start * A1FS_BLOCK_SIZE);
    if (filler(buf, ".", NULL, 0) != 0) { return -ENOMEM; }
    if (filler(buf, "..", NULL, 0) != 0) { return -ENOMEM; }
    while (num_dentries_searched < total_num_dentries) {
        if (filler(buf, dentry->name, NULL, 0) != 0) { return -ENOMEM; }
        dentry += 1;
        if ((a1fs_dentry*) ((void*)sp + A1FS_BLOCK_SIZE * (extent->start + extent->count)) == dentry) {
            // the current extent is already searched completely
            // Forward to the next extent
            extent += 1;
            dentry = (a1fs_dentry*) ((void*)sp + extent->start * A1FS_BLOCK_SIZE);
        }
        num_dentries_searched += 1;
    }
    return 0;
}


/**
 * Create a directory.
 *
 * Implements the mkdir() system call.
 *
 * NOTE: the mode argument may not have the type specification bits set, i.e.
 * S_ISDIR(mode) can be false. To obtain the correct directory type bits use
 * "mode | S_IFDIR".
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the directory to create.
 * @param mode  file mode bits.
 * @return      0 on success; -errno on error.
 */
static int a1fs_mkdir(const char *path, mode_t mode)
{
	fs_ctx *fs = get_fs();

	//TODO: create a directory at given path with given mode
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    if (sp->blocks_count > sp->max_block_count) return -ENOSPC;
    char p_path[A1FS_PATH_MAX];
    char name[A1FS_NAME_MAX];
    find_parent_path(path, p_path, name);
    a1fs_inode* p_inode;
    a1fs_inode* new_inode;
    a1fs_dentry* new_dentry;
    find_inode_from_path(p_path, &p_inode);
    int result = add_data(p_inode, (char**)&new_dentry, sizeof(a1fs_dentry));
    if (result < 0) return result;
    result = make_inode(&new_inode, new_dentry);
    if (result < 0) {
        delete_data(p_inode, p_inode->size - sizeof(a1fs_dentry), sizeof(a1fs_dentry));
        return result;
    }
    strncpy(new_dentry->name, name, A1FS_NAME_MAX);
    new_dentry->name[A1FS_NAME_MAX - 1] = '\0';
    p_inode->links += 1;
    new_inode->links = 2;
    new_inode->mode = mode | S_IFDIR;
    update_time_for_family(path);
    return 0;
}

/**
 * Remove a directory.
 *
 * Implements the rmdir() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOTEMPTY  the directory is not empty.
 *
 * @param path  path to the directory to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_rmdir(const char *path)
{
	fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    a1fs_inode *file_inode;
    find_inode_from_path(path, &file_inode);
    if (file_inode->size != 0) return -ENOTEMPTY;
    // We are going to remove the directory after checking
    update_time_for_family(path);
    char p_path[A1FS_PATH_MAX];
    char name[A1FS_NAME_MAX];
    find_parent_path(path, p_path, name);
    a1fs_inode *p_inode;
    find_inode_from_path(p_path, &p_inode);
    p_inode->links -= 1;
    a1fs_dentry *pos;
    int order = find_dentry_from_inode(p_inode, &pos, name);
    int ino = ((a1fs_dentry*)pos)->ino;
    char* inode_bitmap = (char*) ((void*)sp + sp->inode_bitmap * A1FS_BLOCK_SIZE);
    sp->inodes_count -= 1;
    sp->free_inodes_count += 1;
    free_bitmap(inode_bitmap, ino);
    delete_data(p_inode, sizeof(a1fs_dentry) * order, sizeof(a1fs_dentry));
    return 0;
}

/**
 * Create a file.
 *
 * Implements the open()/creat() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to create.
 * @param mode  file mode bits.
 * @param fi    unused.
 * @return      0 on success; -errno on error.
 */
static int a1fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	(void)fi;// unused
	assert(S_ISREG(mode));
	fs_ctx *fs = get_fs();
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    if (sp->blocks_count > sp->max_block_count) return -ENOSPC;
    char p_path[A1FS_PATH_MAX]; // path to the parent of the file that required to be created
    char name[A1FS_NAME_MAX];   // name of the file
    find_parent_path(path, p_path, name);
    a1fs_inode* p_inode;
    a1fs_inode* new_inode;
    a1fs_dentry* new_dentry;
    find_inode_from_path(p_path, &p_inode);
    int result = add_data(p_inode, (char**) &new_dentry, sizeof(a1fs_dentry));
    if (result < 0) return result;
    result = make_inode(&new_inode, new_dentry);
    if (result < 0) {
        delete_data(p_inode, p_inode->size - sizeof(a1fs_dentry), sizeof(a1fs_dentry));
        return result;
    }
    strncpy(new_dentry->name, name, A1FS_NAME_MAX);
    new_dentry->name[A1FS_NAME_MAX - 1] = '\0';
    new_inode->links = 1;
    new_inode->mode = mode | S_IFREG;
    update_time_for_family(path);
    return 0;
}

/**
 * Remove a file.
 *
 * Implements the unlink() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path  path to the file to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_unlink(const char *path)
{
	fs_ctx *fs = get_fs();
	a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    update_time_for_family(path);
    a1fs_inode *file_inode;
    find_inode_from_path(path, &file_inode);
    char p_path[A1FS_PATH_MAX];
    char name[A1FS_NAME_MAX];
    find_parent_path(path, p_path, name);
    a1fs_inode* p_inode;
    find_inode_from_path(p_path, &p_inode);
    a1fs_dentry *pos;
    int order = find_dentry_from_inode(p_inode, &pos, name);
    int ino = pos->ino;
    char* inode_bitmap = (char*) ((void*)sp + sp->inode_bitmap * A1FS_BLOCK_SIZE);
    sp->inodes_count -= 1;
    sp->free_inodes_count += 1;
    free_bitmap(inode_bitmap, ino);
    delete_data(p_inode, sizeof(a1fs_dentry) * order, sizeof(a1fs_dentry));
    return 0;
}



/**
 * Rename a file or directory.
 *
 * Implements the rename() system call. See "man 2 rename" for details.
 * If the destination file (directory) already exists, it must be replaced with
 * the source file (directory). Existing destination can be replaced if it's a
 * file or an empty directory.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "from" exists.
 *   The parent directory of "to" exists and is a directory.
 *   If "from" is a file and "to" exists, then "to" is also a file.
 *   If "from" is a directory and "to" exists, then "to" is also a directory.
 *
 * Errors:
 *   ENOMEM     not enough memory (e.g. a malloc() call failed).
 *   ENOTEMPTY  destination is a non-empty directory.
 *   ENOSPC     not enough free space in the file system.
 *
 * @param from  original file path.
 * @param to    new file path.
 * @return      0 on success; -errno on error.
 */
static int a1fs_rename(const char *from, const char *to)
{
	//TODO: move the inode (file or directory) at given source path to the
	// destination path, according to the description above
    a1fs_inode *inode_from, *inode_to;
    a1fs_dentry *dentry_from, *dentry_to;
    a1fs_inode *inode_parent_from, *inode_parent_to;
    char path_parent_from[A1FS_PATH_MAX], dentry_name[A1FS_NAME_MAX];
    char path_parent_to[A1FS_PATH_MAX];
    int result;

    // find the orignial inode
    result = find_inode_from_path(from, &inode_from);
    if(result != 0 ){ return result;}

    // find the orignial parent inode
    find_parent_path(from, path_parent_from,dentry_name);
    result = find_inode_from_path(path_parent_from, &inode_parent_from);
    if(result != 0 ){ return result;}

    //find the dentry in orignial parent inode, free this dentry, copy it's info to new dentry
    int from_order = find_dentry_from_inode(inode_parent_from, &dentry_from, dentry_name);

    // find the desternation parent inode
    find_parent_path(to, path_parent_to, dentry_name);
    if (strcmp(path_parent_to, path_parent_from) == 0){
        // if parent directory are same, only need to change the dentry's name
        strcpy(dentry_from->name, dentry_name);
        return 0;
    }

    // if parent directory are different, we need to move the file/dir
    result = find_inode_from_path(path_parent_to, &inode_parent_to);
    if(result != 0 ){ return result;}
    //find the dentry in desternation parent inode
    find_dentry_from_inode(inode_parent_to, &dentry_to, dentry_name);
    if(dentry_to){ 
        result = find_inode_from_path(to, &inode_to);
        if(result != 0 ){ return result;}
        // remove dentry and inode in the path <to>
        // create a new dentry
        if(S_ISDIR(inode_to->mode)) { 
            a1fs_rmdir(to);
        }else { 
            a1fs_unlink(to); 
        }
    }

    //dentry does not exist, create a new dentry in parent_inode_tp
    result = add_data(inode_parent_to, (char**) &dentry_to, sizeof(a1fs_dentry));
    if(result != 0 ){ return result;}
    dentry_to->ino = dentry_from->ino;
    strcpy(dentry_to->name, dentry_name);
    delete_data(inode_parent_from, from_order * sizeof(a1fs_dentry), sizeof(a1fs_dentry));
    inode_parent_from->links -= 1;
    update_time_for_family(path_parent_from);
    update_time_for_family(to);
    return 0;
}


/**
 * Change the access and modification times of a file or directory.
 *
 * Implements the utimensat() system call. See "man 2 utimensat" for details.
 *
 * NOTE: You only have to implement the setting of modification time (mtime).
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists.
 *
 * @param path  path to the file or directory.
 * @param tv    timestamps array. See "man 2 utimensat" for details.
 * @return      0 on success; -errno on failure.
 */
static int a1fs_utimens(const char *path, const struct timespec tv[2])
{    
    a1fs_inode *inode;
    int result = find_inode_from_path(path, &inode);
    if(result != 0){ return result;}
    if(tv[1].tv_nsec == UTIME_NOW){ 
        // current time
        clock_gettime(CLOCK_REALTIME, &inode->mtime);
        return 0;
    }else if (tv[1].tv_nsec == UTIME_OMIT){
        //igore time
        return 0;
    }
    // set time
    inode->mtime.tv_sec = tv[1].tv_sec;
    inode->mtime.tv_nsec = tv[1].tv_nsec;
    return 0;
}


/**
 * Change the size of a file.
 *
 * Implements the truncate() system call. Supports both extending and shrinking.
 * If the file is extended, future reads from the new uninitialized range must
 * return ranges filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to set the size.
 * @param size  new file size in bytes.
 * @return      0 on success; -errno on error.
 */
static int a1fs_truncate(const char *path, off_t size)
{
	//TODO: set new file size, possibly "zeroing out" the uninitialized range
    a1fs_inode * inode;
    int result = find_inode_from_path(path, &inode);
    if(result != 0){ return result; }
    assert(S_ISREG(inode->mode));
    if((uint64_t) size < inode->size){
        // deallocate 
        size_t remaning = inode->size - size;
        delete_data(inode, size, remaning);
    }else if((uint64_t) size> inode->size) {
        // allocate space with 0s
        size_t remaning = size - inode->size;
        char *ptr;
        int result = add_data(inode, &ptr, remaning);
        if(result != 0){ return result;}
    } else return 0;
    update_time_for_family(path);
	return 0;
}


/**
 * Read data from a file.
 *
 * Implements the pread() system call. Should return exactly the number of bytes
 * requested except on EOF (end of file) or error, otherwise the rest of the
 * data will be substituted with zeros. Reads from file ranges that have not
 * been written to must return ranges filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path    path to the file to read from.
 * @param buf     pointer to the buffer that receives the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to read from.
 * @param fi      unused.
 * @return        number of bytes read on success; 0 if offset is beyond EOF;
 *                -errno on error.
 */
static int a1fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	//TODO: read data from the file at given offset into the buffer
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    a1fs_inode* inode;
    int result = find_inode_from_path(path, &inode);
    if (result < 0) return result;
    if ((uint64_t) offset > inode->size) return 0;
    a1fs_extent *extent;
    char *curr_ptr;
    find_ptr_at_size(inode, offset, &curr_ptr, &extent);
    // Now curr_ptr should point to the bit offset from the beginning
    uint64_t buf_index = 0;
    uint64_t byte_searched = 0;
    // uint64_t cpy_amt_once;
    while (buf_index < size && byte_searched < inode->size-offset) {
        if ((char*) ((void*)sp + A1FS_BLOCK_SIZE * (extent->start + extent->count)) == curr_ptr) {
            // the current extent is already searched completely
            // Forward to the next extent
            extent += 1;
        }
        buf[buf_index] = *curr_ptr;
        buf_index += 1;
        curr_ptr += 1;
        byte_searched += 1;
    }
    for (uint64_t i=buf_index; i < size; i++) {
        buf[i] = '\0'; 
    }
    return buf_index;
}

/**
 * Write data to a file.
 *
 * Implements the pwrite() system call. Should return exactly the number of
 * bytes requested except on error. If the offset is beyond EOF (end of file),
 * the file must be extended. If the write creates a "hole" of uninitialized
 * data, future reads from the "hole" must return ranges filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path    path to the file to write to.
 * @param buf     pointer to the buffer containing the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to write to.
 * @param fi      unused.
 * @return        number of bytes written on success; -errno on error.
 */
static int a1fs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();
	// write data from the buffer into the file at given offset, possibly
    a1fs_superblock *sp = (a1fs_superblock*)fs->image;
    a1fs_inode* inode;
    int result = find_inode_from_path(path, &inode);
    if (result < 0) return result;
    // "zeroing out" the uninitialized range
    a1fs_truncate(path, (size_t)offset + size);

    char *curr_ptr;
    a1fs_extent * extent;
    find_ptr_at_size(inode, offset, &curr_ptr, &extent);
    size_t byte_written = 0;
    // write one byte each time
    while (byte_written < size) {
        if ((char*) ((void*)sp + A1FS_BLOCK_SIZE * (extent->start + extent->count)) == curr_ptr) {
            // Forward to the next extent
            extent += 1;
            curr_ptr = (char*) ((void*)sp + extent->start * A1FS_BLOCK_SIZE);
        }
        *curr_ptr = buf[byte_written];
        byte_written += 1;
        curr_ptr += 1;
    }
    update_time_for_family(path);
    return byte_written;
}


static struct fuse_operations a1fs_ops = {
	.destroy  = a1fs_destroy, 
	.statfs   = a1fs_statfs,
	.getattr  = a1fs_getattr,
	.readdir  = a1fs_readdir,
	.mkdir    = a1fs_mkdir,
	.rmdir    = a1fs_rmdir,
	.create   = a1fs_create,
	.unlink   = a1fs_unlink,
	.rename   = a1fs_rename,
	.utimens  = a1fs_utimens,
	.truncate = a1fs_truncate,
	.read     = a1fs_read,
	.write    = a1fs_write,
};

int main(int argc, char *argv[])
{
	a1fs_opts opts = {0};// defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!a1fs_opt_parse(&args, &opts)) return 1;

	fs_ctx fs = {0};
	if (!a1fs_init(&fs, &opts)) {
		fprintf(stderr, "Failed to mount the file system\n");
		return 1;
	}

	return fuse_main(args.argc, args.argv, &a1fs_ops, &fs);
}
