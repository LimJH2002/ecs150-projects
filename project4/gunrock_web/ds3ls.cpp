#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#include "StringUtils.h"
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

bool compareByName(const dir_ent_t &a, const dir_ent_t &b)
{
  return strcmp(a.name, b.name) < 0;
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    cerr << "Directory not found" << endl;
    return 1;
  }

  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string directory = string(argv[2]);

  try
  {
    int currentInode = UFS_ROOT_DIRECTORY_INODE_NUMBER;

    if (directory != "/")
    {
      if (directory[0] == '/')
      {
        directory = directory.substr(1);
      }
      vector<string> parts = StringUtils::split(directory, '/');
      for (size_t i = 0; i < parts.size(); i++)
      {
        int nextInode = fileSystem->lookup(currentInode, parts[i]);
        if (nextInode < 0)
        {
          cerr << "Directory not found" << endl;
          delete fileSystem;
          delete disk;
          return 1;
        }
        currentInode = nextInode;
      }
    }

    // Get inode info
    inode_t inode;
    if (fileSystem->stat(currentInode, &inode) < 0)
    {
      cerr << "Directory not found" << endl;
      delete fileSystem;
      delete disk;
      return 1;
    }

    // Handle directory listing
    if (inode.type == UFS_DIRECTORY)
    {
      // Read directory entries
      vector<dir_ent_t> entries(inode.size / sizeof(dir_ent_t));
      if (fileSystem->read(currentInode, &entries[0], inode.size) < 0)
      {
        cerr << "Directory not found" << endl;
        delete fileSystem;
        delete disk;
        return 1;
      }

      // Sort and print entries
      sort(entries.begin(), entries.end(), compareByName);
      for (size_t i = 0; i < entries.size(); i++)
      {
        cout << entries[i].inum << "\t" << entries[i].name << endl;
      }
    }
    else
    {
      // Print file entry
      string filename = directory;
      if (directory != "/")
      {
        size_t pos = directory.find_last_of('/');
        filename = (pos != string::npos) ? directory.substr(pos + 1) : directory;
      }
      cout << currentInode << "\t" << filename << endl;
    }

    delete fileSystem;
    delete disk;
    return 0;
  }
  catch (...)
  {
    cerr << "Directory not found" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
}