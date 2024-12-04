#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <cstring>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

LocalFileSystem::LocalFileSystem(Disk *disk)
{
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super)
{
  unsigned char buffer[UFS_BLOCK_SIZE];

  // Read the block into our buffer
  disk->readBlock(0, buffer);

  // Copy just the superblock portion
  memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
  // Read all blocks of the inode bitmap
  for (int i = 0; i < super->inode_bitmap_len; i++)
  {
    disk->readBlock(super->inode_bitmap_addr + i, inodeBitmap + (i * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap)
{
  // Read all blocks of the data bitmap
  for (int i = 0; i < super->data_bitmap_len; i++)
  {
    disk->readBlock(super->data_bitmap_addr + i, dataBitmap + (i * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap)
{
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes)
{
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes)
{
}

int LocalFileSystem::lookup(int parentInodeNumber, string name)
{
  // Get parent directory's inode
  inode_t inode;
  int result = stat(parentInodeNumber, &inode);
  if (result < 0)
    return result;

  // Must be a directory
  if (inode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE;
  }

  // Read directory entries
  char buffer[inode.size];
  result = read(parentInodeNumber, buffer, inode.size);
  if (result < 0)
    return result;

  // Search for matching name
  dir_ent_t *entries = (dir_ent_t *)buffer;
  int numEntries = inode.size / sizeof(dir_ent_t);

  for (int i = 0; i < numEntries; i++)
  {
    if (name == entries[i].name)
    {
      return entries[i].inum;
    }
  }

  return -ENOTFOUND;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode)
{
  // Read superblock first
  super_t super;
  readSuperBlock(&super);

  // Validate inode number
  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

  // Calculate location of inode
  int inodeOffset = inodeNumber * sizeof(inode_t);
  int blockNum = super.inode_region_addr + (inodeOffset / UFS_BLOCK_SIZE);

  // Read the block containing this inode
  inode_t inodeBlock[UFS_BLOCK_SIZE / sizeof(inode_t)];
  disk->readBlock(blockNum, inodeBlock);

  // Copy the specific inode
  int inodeIndex = (inodeOffset % UFS_BLOCK_SIZE) / sizeof(inode_t);
  *inode = inodeBlock[inodeIndex];

  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size)
{
  if (size < 0)
    return -EINVALIDSIZE;

  inode_t inode;
  int result = stat(inodeNumber, &inode);
  if (result < 0)
    return result;

  // Read from direct blocks
  unsigned char blockBuffer[UFS_BLOCK_SIZE];
  int bytesRead = 0;
  int remaining = (size < inode.size) ? size : inode.size;

  while (remaining > 0 && bytesRead < inode.size)
  {
    int blockNum = bytesRead / UFS_BLOCK_SIZE;
    if (blockNum >= DIRECT_PTRS)
      break;

    disk->readBlock(inode.direct[blockNum], blockBuffer);

    int copySize = (remaining < UFS_BLOCK_SIZE) ? remaining : UFS_BLOCK_SIZE;
    memcpy((char *)buffer + bytesRead, blockBuffer, copySize);

    bytesRead += copySize;
    remaining -= copySize;
  }

  return bytesRead;
}

// Add these helper methods first
int findFreeInodeNumber(super_t *super, unsigned char *bitmap)
{
  for (int i = 0; i < super->num_inodes; i++)
  {
    if (!(bitmap[i / 8] & (1 << (i % 8))))
    {
      bitmap[i / 8] |= (1 << (i % 8));
      return i;
    }
  }
  return -ENOTENOUGHSPACE;
}

int findFreeDataBlock(super_t *super, unsigned char *bitmap)
{
  for (int i = 0; i < super->num_data; i++)
  {
    if (!(bitmap[i / 8] & (1 << (i % 8))))
    {
      bitmap[i / 8] |= (1 << (i % 8));
      return i;
    }
  }
  return -ENOTENOUGHSPACE;
}

// Main create implementation
int LocalFileSystem::create(int parentInodeNumber, int type, string name)
{
  if (name.length() >= DIR_ENT_NAME_SIZE)
  {
    return -EINVALIDNAME;
  }

  // Read superblock
  super_t super;
  readSuperBlock(&super);

  // Verify parent inode
  inode_t parentInode;
  int result = stat(parentInodeNumber, &parentInode);
  if (result < 0)
    return result;
  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE;
  }

  // Check if entry already exists
  try
  {
    int existingInode = lookup(parentInodeNumber, name);
    inode_t existing;
    stat(existingInode, &existing);
    if (existing.type != type)
    {
      return -EINVALIDTYPE;
    }
    return existingInode; // Return existing inode if type matches
  }
  catch (...)
  {
    // Entry doesn't exist, continue with creation
  }

  disk->beginTransaction();

  try
  {
    // Allocate new inode
    unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
    readInodeBitmap(&super, inodeBitmap);
    int newInodeNum = findFreeInodeNumber(&super, inodeBitmap);
    if (newInodeNum < 0)
    {
      disk->rollback();
      return -ENOTENOUGHSPACE;
    }
    writeInodeBitmap(&super, inodeBitmap);

    // Allocate data block
    unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
    readDataBitmap(&super, dataBitmap);
    int dataBlock = findFreeDataBlock(&super, dataBitmap);
    if (dataBlock < 0)
    {
      disk->rollback();
      return -ENOTENOUGHSPACE;
    }
    writeDataBitmap(&super, dataBitmap);

    // Initialize inode
    inode_t newInode;
    memset(&newInode, 0, sizeof(inode_t));
    newInode.type = type;
    newInode.direct[0] = super.data_region_addr + dataBlock;

    if (type == UFS_DIRECTORY)
    {
      // Set up directory entries for "." and ".."
      dir_ent_t entries[2];
      strcpy(entries[0].name, ".");
      entries[0].inum = newInodeNum;
      strcpy(entries[1].name, "..");
      entries[1].inum = parentInodeNumber;

      newInode.size = 2 * sizeof(dir_ent_t);
      disk->writeBlock(newInode.direct[0], entries);
    }

    // Write new inode
    unsigned char inodeBlock[UFS_BLOCK_SIZE];
    disk->readBlock(super.inode_region_addr + newInodeNum / (UFS_BLOCK_SIZE / sizeof(inode_t)), inodeBlock);
    memcpy(inodeBlock + (newInodeNum % (UFS_BLOCK_SIZE / sizeof(inode_t))) * sizeof(inode_t),
           &newInode, sizeof(inode_t));
    disk->writeBlock(super.inode_region_addr + newInodeNum / (UFS_BLOCK_SIZE / sizeof(inode_t)), inodeBlock);

    // Add entry to parent directory
    char parentData[parentInode.size + sizeof(dir_ent_t)];
    read(parentInodeNumber, parentData, parentInode.size);

    dir_ent_t *newEntry = (dir_ent_t *)(parentData + parentInode.size);
    strcpy(newEntry->name, name.c_str());
    newEntry->inum = newInodeNum;

    parentInode.size += sizeof(dir_ent_t);
    write(parentInodeNumber, parentData, parentInode.size);

    disk->commit();
    return newInodeNum;
  }
  catch (...)
  {
    disk->rollback();
    return -ENOTENOUGHSPACE;
  }
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size)
{
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name)
{
  return 0;
}
