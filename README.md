## Using FUSE API to implement a File system.
1. Use `$ make` to compile.
2. `$ sh runit.sh`  to run the demo, or use `>` to indirect the result

PS: There are some helper functions in `helper.h` and `helper.c` because `mkfs.c `and a1fs.c both use them.

Basic Information:
-All file/directory are empty when created.(i.e. size 0)
-All file/directory do not have direct pointer: if a file/directory is not empty, it can have up to one indirect extent
-The indirect extent is stored as "extents" inside inode, its length is fixed to be 1. containing up to 512 direct extents
-The data are consistent: All data blocks of a file/directory except the last data block, is filled with data.
-The first inode in inode table is preserved for error handle. The second inode is inode of root.
-No valid dentry has inode number 0.
-Inode.blocks count all blocks used by this inode(including indirect block).
-Block bitmap start from superblock, so first few blocks should be set already when formatting.
-The file system at least need 4 blocks to be initialized

# Mount the File System:
```bash
make
truncate -s <size> <img>
./mkfs.a1fs -i <num_ino> <img>
```

# Run without gdb:
`   ./a1fs <img> <mount point>  `
with `gdb`:
`   gdb --args ./a1fs <img> <mount point>  `

# Basic Operations:
```bash
mkdir  <dir>
touch  <file>
mv  <from>  <to>
rm  <file>
rm  -r  <dir>
rmdir <dir>
cat   <file>
cat   <file>  >  <file>
cat   <file>  >>  <file>
echo  <text>  >   <file>
echo  <text> >>   <file>
truncate  -s  <size>  <file>
```

# Limits and details

- The maximum number of inodes in the system is a parameter to `mkfs.a1fs`, the image size is also known to it, and the block size is `A1FS_BLOCK_SIZE` (4096 bytes). Many parameters of your file system can be computed from these three values.
- Your file system does not have to support more then 512 extents in a file.
- We will not test your code on an image smaller than 64 KiB (16 blocks) with 4 inodes. You should be able to fit the root directory and a non-empty file in an image of this size and configuration. You shouldn't pre-allocate additional space for metadata (beyond what's necessary to store the inodes and the information about free and available blocks and inodes) in your `mkfs.a1fs` implementation.
- The maximum path component length is `A1FS_NAME_MAX` (252 bytes including the null terminator). This value is chosen to fit the directory entry structure into 256 bytes (see `a1fs.h`). Names stored in directory entries are null-terminated string so that you can use standard C string functions on them.
- The maximum full path length is `PATH_MAX` (4096 bytes including the null terminator). This allows you to use fixed-size buffers for operations like splitting a path into a directory name and a file name.
- There are no limits on the number of directory entries (except as dictated by the number of extents per file).
- There are no limits on the number of blocks in your file system (except for what can be addressed with 32-bit block pointers and 4096-byte block size).

Sample disk configurations that must work include:

- 64KiB size and 4 inodes
- 64KiB size and 16 inodes
- 1MiB size and 128 inodes
- 10MiB size and 4096 inodes
- 2GiB size and 4096 inodes

We will not be testing your code under extreme circumstances so don't get carried away by thinking about corner cases. However, we do expect you to properly handle "out of space" conditions in your code.

# Understanding the code

First read through all of the starter code to understand how it fits together, and which files contain helper functions that will be useful in your implementation.

- `mkfs.c` - contains the program to format your disk image. You need to write part of this program.
- `a1fs.h` - contains the data structure definitions and constants needed for the file system. You wrote this for your proposal, but you may find you need to make changes as you work on the implementation. Also, we've added some useful documentation comments since the a1a version of this file, so please make sure to use the updated version.
- `a1fs.c` - contains the program used to mount your file system. This includes the callback functions that will implement the underlying file system operations. Each function that you will implement is preceded by detailed comments and has a "TODO" in it. Please read this file carefully.

**NOTE:** It is very important to return the correct error codes (or 0 on success) from all the FUSE callback functions, according to the "Errors" section in the comment above the function. The FUSE library, the kernel, and the user-space tools used to access the file system all rely on these return codes for correctness of operation.

Note: You will see many lines like `(void)fs;`. Their purpose is to prevent the compiler from warning about unused variables. You should delete these lines as you make use of the variables.

- `fs_ctx.h` and `fs_ctx.c` - The `fs_ctx` struct contains runtime state of your mounted file system. Any time you think you need a global variable, it should go in this struct instead.
- `map.h` and `map.c` - contain the `map_file()` function used by `a1fs` and `mkfs.a1fs` to map the image file into memory and determine its size.
- `options.h` and `options.c` - contain the code to parse command line arguments for the `a1fs` program.
- `util.h` - contains some handy functions.