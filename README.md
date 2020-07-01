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
