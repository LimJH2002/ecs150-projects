#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    cerr << argv[0] << ": diskImageFile inodeNumber" << endl;
    return 1;
  }

  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int inodeNumber = stoi(argv[2]);

  try
  {
    inode_t inode;
    if (fileSystem->stat(inodeNumber, &inode) < 0)
    {
      cerr << "Error reading file" << endl;
      delete fileSystem;
      delete disk;
      return 1;
    }

    if (inode.type == UFS_DIRECTORY)
    {
      cerr << "Error reading file" << endl;
      delete fileSystem;
      delete disk;
      return 1;
    }

    // Calculate number of blocks needed based on file size
    int numBlocks = (inode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

    cout << "File blocks" << endl;
    for (int i = 0; i < numBlocks; i++)
    {
      cout << inode.direct[i] << endl;
    }
    cout << endl;

    cout << "File data" << endl;
    char buffer[inode.size];
    int bytesRead = fileSystem->read(inodeNumber, buffer, inode.size);
    if (bytesRead < 0)
    {
      cerr << "Error reading file" << endl;
      delete fileSystem;
      delete disk;
      return 1;
    }
    cout.write(buffer, inode.size);

    delete fileSystem;
    delete disk;
    return 0;
  }
  catch (...)
  {
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
}