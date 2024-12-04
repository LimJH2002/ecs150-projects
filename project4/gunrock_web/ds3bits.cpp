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
  if (argc != 2)
  {
    cerr << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);

  try
  {
    super_t super;
    fileSystem->readSuperBlock(&super);

    cout << "Super" << endl;
    cout << "inode_region_addr " << super.inode_region_addr << endl;
    cout << "inode_region_len " << super.inode_region_len << endl;
    cout << "num_inodes " << super.num_inodes << endl;
    cout << "data_region_addr " << super.data_region_addr << endl;
    cout << "data_region_len " << super.data_region_len << endl;
    cout << "num_data " << super.num_data << endl;
    cout << endl;

    // Calculate actual bitmap sizes in bytes
    int inodeBitmapBytes = (super.num_inodes + 7) / 8; // Round up to nearest byte
    int dataBitmapBytes = (super.num_data + 7) / 8;    // Round up to nearest byte

    // Read and print inode bitmap
    unsigned char inodeBitmap[UFS_BLOCK_SIZE];
    fileSystem->readInodeBitmap(&super, inodeBitmap);
    cout << "Inode bitmap" << endl;
    for (int i = 0; i < inodeBitmapBytes; i++)
    {
      cout << (unsigned int)inodeBitmap[i];
      if (i < inodeBitmapBytes - 1)
        cout << " ";
    }
    cout << " " << endl
         << endl;

    // Read and print data bitmap
    unsigned char dataBitmap[UFS_BLOCK_SIZE];
    fileSystem->readDataBitmap(&super, dataBitmap);
    cout << "Data bitmap" << endl;
    for (int i = 0; i < dataBitmapBytes; i++)
    {
      cout << (unsigned int)dataBitmap[i];
      if (i < dataBitmapBytes - 1)
        cout << " ";
    }
    cout << " " << endl;

    delete fileSystem;
    delete disk;
    return 0;
  }
  catch (...)
  {
    cerr << "Error reading disk image" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
}