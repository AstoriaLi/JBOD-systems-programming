/* Author: Yunyun (Astoria) Li   
   Date: 4/24/2024
    */
    
    
    
/***
 *      ______ .___  ___. .______     _______.  ______              ____    __   __  
 *     /      ||   \/   | |   _  \   /       | /      |            |___ \  /_ | /_ | 
 *    |  ,----'|  \  /  | |  |_)  | |   (----`|  ,----'              __) |  | |  | | 
 *    |  |     |  |\/|  | |   ___/   \   \    |  |                  |__ <   | |  | | 
 *    |  `----.|  |  |  | |  |   .----)   |   |  `----.             ___) |  | |  | | 
 *     \______||__|  |__| | _|   |_______/     \______|            |____/   |_|  |_| 
 *                                                                                   
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "util.h"
#include "jbod.h"
#include "cache.h"
#include "net.h"

int is_mounted = 0;

/*
uint32_t helper(uint32_t diskID, uint32_t blockID, uint32_t command){
  uint32_t result = 0;
  uint32_t tempDiskID = diskID << 28;
  uint32_t tempBlockID = blockID << 20;
  uint32_t tempCommand = command << 14;
  result = tempDiskID | tempBlockID | tempCommand;
  return result;
}
*/

// creates op code
uint32_t helper(uint32_t command, uint32_t diskID, uint32_t reserved, uint32_t blockID){
  uint32_t opCode = 0;
  
  /*
  command = command << 26;
  diskID = diskID <<  22;
  reserved = reserved << 8;
  */
  
  diskID = diskID << 28;
  blockID = blockID << 20;
  command = command << 14;
  
  opCode = opCode | command | diskID | reserved | blockID;
  return opCode;
}


/*
uint32_t opCode(uint32_t command, uint32_t diskID, uint32_t reserved, uint32_t blockID){
  // construct a 32 bit op code from given information
  uint32_t code = 0;
  command = command << 26;
  
}
*/

int mdadm_mount(void) {
  // check if it's mounted
  if (is_mounted == 1){
    return -1;
  }
  
  jbod_cmd_t op = helper(JBOD_MOUNT, 0, 0, 0);
  
  jbod_client_operation(op, NULL);
  is_mounted = 1;
  return 1;
}

int mdadm_unmount(void) {
  // check if it's unmounted
  if (is_mounted == 0){
    return -1;
  }
  
  jbod_cmd_t op = helper(JBOD_UNMOUNT, 0, 0, 0);
  
  jbod_client_operation(op, NULL);
  is_mounted = 0;
  return 1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  // check for invalid parameters
  if (buf == NULL){
    if (len == 0){
      return len;
    }
    return -1;
  }
  
  if (len > 1024 || addr + len > (JBOD_NUM_DISKS * JBOD_DISK_SIZE) || addr < 0){
    return -1;
  }
  
  if (is_mounted == 0){
    return -1;
  }
  
  // current disk & block & byte
  // currLen: the length we have read so far
  int diskID = addr / JBOD_DISK_SIZE;
  int blockID = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  int currByte = addr % JBOD_BLOCK_SIZE;
  int target = addr + len;
  int cacheFlag;
  
  // out array stores the read values (at most 256 values)
  // outPtr is a pointer pointing to out
  uint8_t out[JBOD_BLOCK_SIZE];
  uint8_t *outPtr; 
  outPtr = &out[currByte];
  
  jbod_cmd_t op_seek_disk = helper(JBOD_SEEK_TO_DISK, diskID, 0, 0);
  jbod_cmd_t op_seek_block = helper(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
  jbod_cmd_t op_read_block = helper(JBOD_READ_BLOCK, 0, 0, 0);
  
  // locate current disk & block
  jbod_client_operation(op_seek_disk, NULL);
  jbod_client_operation(op_seek_block, NULL);
  
  if(cache_enabled())
  {
    cacheFlag = cache_lookup(diskID, blockID, out);
    if(cacheFlag == -1)
    {
      // read the block
      jbod_client_operation(op_read_block, out);
      
      cache_insert(diskID, blockID, out);
    }
  }
  else
  {
    // read the first (current) block
    jbod_client_operation(op_read_block, out);
  }
  
  while (addr < target){
    memcpy(buf, outPtr, 1);
    buf++;
    outPtr++;
    
    // if we reach end of disk
    if ((addr % JBOD_DISK_SIZE) == JBOD_DISK_SIZE - 1){
      diskID++;
      blockID = 0;
      
      op_seek_disk = helper(JBOD_SEEK_TO_DISK, diskID, 0, 0);
      op_seek_block = helper(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
      
      // locate current disk & block
      jbod_client_operation(op_seek_disk, NULL);
      jbod_client_operation(op_seek_block, NULL);
      
      if(cache_enabled())
      {
        cacheFlag = cache_lookup(diskID, blockID, out);
        if(cacheFlag == -1)
        {
          // read the block
          jbod_client_operation(op_read_block, out);
      
          cache_insert(diskID, blockID, out);
        }
      }
      else
      {
        // read the first (current) block
        jbod_client_operation(op_read_block, out);
      }
      
      outPtr = out;
    }
    // if we reach end of block
    else if ((addr % JBOD_BLOCK_SIZE) == JBOD_BLOCK_SIZE - 1){
      blockID++;
      
      // locate current block
      op_seek_block = helper(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
      jbod_client_operation(op_seek_block, out);
      
      if(cache_enabled())
      {
        cacheFlag = cache_lookup(diskID, blockID, out);
        if(cacheFlag == -1)
        {
          // read the block
          jbod_client_operation(op_read_block, out);
          
          cache_insert(diskID, blockID, out);
        }
      }
      else
      {
        // read the first (current) block
        jbod_client_operation(op_read_block, out);
      }
      
      outPtr = out;
    }
    addr++;
  }
  
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  if (buf == NULL){
    if (len == 0){
      return len;
    }
    return -1;
  }
  
  if (len > 1024 || addr + len > (JBOD_NUM_DISKS * JBOD_DISK_SIZE) || addr < 0){
    return -1;
  }
  
  if (is_mounted == 0){
      return -1;
  }
  
  // current disk & block & byte
  // currLen: the length we have read so far
  int diskID = addr / JBOD_DISK_SIZE;
  int blockID = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  int currByte = addr % JBOD_BLOCK_SIZE;
  int target = addr + len;
  int cacheFlag;
  
  // writeVal array stores the values that need to be written (at most 256 values)
  // writePtr is a pointer pointing to writeVal
  uint8_t writeVal[JBOD_BLOCK_SIZE];
  uint8_t *writePtr; 
  writePtr = &writeVal[currByte];
  
  uint8_t *dummy = (uint8_t *) buf;
  
  jbod_cmd_t op_seek_disk = helper(JBOD_SEEK_TO_DISK, diskID, 0, 0);
  jbod_cmd_t op_seek_block = helper(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
  jbod_cmd_t op_read_block = helper(JBOD_READ_BLOCK, 0, 0, 0);
  jbod_cmd_t op_write_block = helper(JBOD_WRITE_BLOCK, 0, 0, 0);
  
  // locate current disk & block
  jbod_client_operation(op_seek_disk, NULL);
  jbod_client_operation(op_seek_block, NULL);
  
  if(cache_enabled())
  {
    cacheFlag = cache_lookup(diskID, blockID, writeVal);
    if(cacheFlag == -1)
    {
      // read values that need to be written into writeVal
      jbod_client_operation(op_read_block, writeVal);
      
      // locate current block
      jbod_client_operation(op_seek_block, NULL);
      
      cache_insert(diskID, blockID, writeVal);
    }
  }
  else
  {
    // read values that need to be written into writeVal
    jbod_client_operation(op_read_block, writeVal);
    
    // locate current block
    jbod_client_operation(op_seek_block, NULL);
  }
  
  while (addr < target){
    memcpy(writePtr, dummy, 1);
    dummy++;
    writePtr++;
    
    // if we reach end of disk
    if ((addr % JBOD_DISK_SIZE) == JBOD_DISK_SIZE - 1){
      // write value
      jbod_client_operation(op_write_block, writeVal);
      
      if(cache_enabled())
      {
        cache_update(diskID, blockID, writeVal);
      }
      
      diskID++;
      blockID = 0;
      
      op_seek_disk = helper(JBOD_SEEK_TO_DISK, diskID, 0, 0);
      op_seek_block = helper(JBOD_SEEK_TO_BLOCK, 0, 0, 0);
      
      // locate current disk & block
      jbod_client_operation(op_seek_disk, NULL);
      jbod_client_operation(op_seek_block, NULL);
      
      if(cache_enabled())
      {
        cacheFlag = cache_lookup(diskID, blockID, writeVal);
        if(cacheFlag == -1)
        {
          op_read_block = helper(JBOD_READ_BLOCK, 0, 0, 0);
          op_write_block = helper(JBOD_WRITE_BLOCK, 0, 0, 0);
          
          // read values that need to be written into writeValE_BLOCK, 0, 0, 0);
          jbod_client_operation(op_read_block, writeVal);
          
          cache_insert(diskID, blockID, writeVal);
	  
          // locate current block
          jbod_client_operation(op_seek_block, NULL);
        }
      }
      else
      {
        op_read_block = helper(JBOD_READ_BLOCK, 0, 0, 0);
        op_write_block = helper(JBOD_WRITE_BLOCK, 0, 0, 0);
          
        // read values that need to be written into writeValE_BLOCK, 0, 0, 0);
        jbod_client_operation(op_read_block, writeVal);
          
        // locate current block
        jbod_client_operation(op_seek_block, NULL);
      }
      
      writePtr = writeVal;
    }
    
    // if we reach end of block
    else if ((addr % JBOD_BLOCK_SIZE) == JBOD_BLOCK_SIZE - 1){
      // write value
      jbod_client_operation(op_write_block, writeVal);
      
      if(cache_enabled())
      {
        cache_update(diskID, blockID, writeVal);
      }
      
      blockID++;
      
      op_seek_disk = helper(JBOD_SEEK_TO_DISK, diskID, 0, 0);
      op_seek_block = helper(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
      
      // locate current block
      jbod_client_operation(op_seek_block, NULL);
      
      if(cache_enabled())
      {
        cacheFlag = cache_lookup(diskID, blockID, writeVal);
        if(cacheFlag == -1)
        {
          // read values that need to be written into writeVal
          jbod_client_operation(op_read_block, writeVal);
          
          // locate current block
          jbod_client_operation(op_seek_block, writeVal);
          
          cache_insert(diskID, blockID, writeVal);
          
          // locate current disk
          jbod_client_operation(op_seek_block, writeVal);
        }
      }
      else
      {
        // read values that need to be written into writeVal
        jbod_client_operation(op_read_block, writeVal);
        
        // locate current block
	jbod_client_operation(op_seek_block, writeVal);
      }
      
      writePtr = writeVal;
    }
    addr++;
  }
  
  // update cache
  if(cache_enabled())
  {
    cache_update(diskID, blockID, writeVal);
  }
  
  // write one last time
  jbod_client_operation(op_write_block, writeVal);
  
  // locate current block
  jbod_client_operation(op_seek_block, writeVal);
  
  return len;
}
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
/*CWD /home/cmpsc311/311/sp24-lab4-AstoriaLi */
