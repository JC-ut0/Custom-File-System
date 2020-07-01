#!/bin/bash
echo "===================== Star demo ====================="
echo "making mount directory at /tmp/mnt"
echo "mkdir /tmp/mnt/"
mkdir /tmp/mnt/
echo "truncate -s 64M img"
truncate -s 64M img
echo "./mkfs.a1fs -i 4096 img"
./mkfs.a1fs -i 4096 img
echo "./a1fs img /tmp/mnt"
./a1fs img /tmp/mnt
echo ""
echo "Gets the information about the root directory"
echo "ls -la /tmp/mnt"
ls -la /tmp/mnt
echo ""
echo "stat /tmp/mnt"
stat /tmp/mnt
echo ""
echo ""
echo "Making two directories"
echo "mkdir /tmp/mnt/dir1"
mkdir /tmp/mnt/dir1
echo "mkdir /tmp/mnt/dir2"
mkdir /tmp/mnt/dir2
echo ""
echo "Creates an empty file in dir1"
echo "touch /tmp/mnt/dir1/file1"
touch /tmp/mnt/dir1/file1
echo ""
echo "Checks the information about dir to make sure file1 is successfully created"
echo "ls -al /tmp/mnt/dir1"
ls -al /tmp/mnt/dir1
echo ""
echo "stat /tmp/mnt/dir1/file1"
stat /tmp/mnt/dir1/file1
echo ""
echo ""
echo "Moves file1 from dir1 to dir2 and rename it to FILE1"
echo "mv /tmp/mnt/dir1/file1 /tmp/mnt/dir2/FILE1"
mv /tmp/mnt/dir1/file1 /tmp/mnt/dir2/FILE1
echo "Checks the information about both dir to make sure file1 is successfully renamed"
echo "ls -al /tmp/mnt/dir1"
ls -al /tmp/mnt/dir1
echo ""
echo "ls -al /tmp/mnt/dir2"
ls -al /tmp/mnt/dir2
# echo "replace empty dirctory"
echo ""
echo "Write message to FILE1 and checks the size of FILE1"
echo "Displays the bytes written in FILE1"
echo "echo 'CSC369 Assignment1 Test Message.' >> /tmp/mnt/dir2/FILE1"
echo "CSC369 Assignment1 Test Message. " >> /tmp/mnt/dir2/FILE1
echo "ls -al /tmp/mnt/dir2"
ls -al /tmp/mnt/dir2
echo "xxd /tmp/mnt/dir2/FILE1"
xxd /tmp/mnt/dir2/FILE1
echo ""
echo "Append a long message to FILE1 and display the bytes in it"
echo "This is a very loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong message." >> /tmp/mnt/dir2/FILE1
echo ""
xxd /tmp/mnt/dir2/FILE1
echo ""
ls -al /tmp/mnt/dir2
echo ""
echo "Unlinks FILE1 and removes both dir1 and dir2."
echo "unlink /tmp/mnt/dir2/FILE1"
unlink /tmp/mnt/dir2/FILE1
echo "rmdir /tmp/mnt/dir2"
rmdir /tmp/mnt/dir2
echo "rmdir /tmp/mnt/dir1"
rmdir /tmp/mnt/dir1
echo ""
echo "Use ls to check both directories are removed"
echo "ls -al /tmp/mnt/"
ls -al /tmp/mnt/
echo ""
echo "Creates and extends a file called test with size of 5000 bytes using truncate"
echo "truncate -s 5000 /tmp/mnt/test"
truncate -s 5000 /tmp/mnt/test
echo ""
echo "ls -al /tmp/mnt"
ls -al /tmp/mnt
echo ""
echo "Append bytes to the end test"
echo "WOW!!! CSC369!" >> /tmp/mnt/test
echo "Use xxd to display the bytes in test"
echo ""
echo "xxd /tmp/mnt/test"
xxd /tmp/mnt/test
echo ""
echo "Use truncate to shrink the file to 100 bytes, and append some bytes to the end of it."
echo "truncate -s 100 /tmp/mnt/test"
truncate -s 100 /tmp/mnt/test
echo "CSC369!!!!!" >> /tmp/mnt/test
echo "Use xxd to display the bytes."
echo "xxd /tmp/mnt/test"
xxd /tmp/mnt/test
echo ""
echo "Unmount filesystem and mount back."
echo "Use ls to show that test is still there and has same size as before."
echo "Use xxd to display the bytes in test is not changed."
echo ""
echo "fusermount -u /tmp/mnt"
fusermount -u /tmp/mnt
echo "./a1fs img /tmp/mnt"
./a1fs img /tmp/mnt
echo "ls -al /tmp/mnt"
ls -al /tmp/mnt
echo "xxd /tmp/mnt/test"
xxd /tmp/mnt/test
echo "===================== End demo ====================="
# fusermount -u /tmp/mnt
# rm -rf /tmp/mnt
# rm img
# echo "Filesystem unmounted, mountpoint directory deleted"
