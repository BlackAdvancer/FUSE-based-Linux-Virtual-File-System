# FUSE-based-Linux-Virtual-File-System
It is a school project that about the Linux file system <br />
To run the file system above, you must use the following command: <br />
```
./ext2fs -f -s -o use_ino mountpoint file.img
```
where mountpoint is the directory where you will mount your file system (details below), and file.img is the file containing the ext2 volume where the data used by the file system is obtained. <br />
Currently, the file system suppot these functions: <br />
```
stat FILE
ls FILE
cat FILE
readlink FILE
```
