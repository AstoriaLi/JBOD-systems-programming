#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static cache_entry_t *currCache;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;
static int numFilledCache = 0;
static bool cacheExist = false;
static bool cacheFull = false;
static bool cacheEmpty = true;

int cache_create(int num_entries) {
  if((num_entries < 2) | (num_entries >4096) | cacheExist){
    return -1;
  }
  
  // allocate memory for cache with size num_entries
  cache_size = num_entries;
  cache = malloc(cache_size * sizeof(cache_entry_t));
  currCache = cache;
  cacheExist = true;
  return 1;
}

int cache_destroy(void) {
  if(!cacheExist)
  {
    return -1;
  }
  
  // free the cache and reset relevant variables
  free(cache);
  cache = NULL;
  currCache = NULL;
  cache_size = 0;
  cacheExist = false;
  numFilledCache = 0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  num_queries ++;
  if((cache == NULL) | cacheEmpty | (buf == NULL))
  {
    return -1;
  }
  
  // where we are in the cache
  cache_entry_t *currCacheIndex;
  currCacheIndex = cache;
  
  for (int i = 0; i < cache_size; i++)
  {
    if((currCacheIndex->disk_num == disk_num) & (currCacheIndex->block_num == block_num))
    {
      // cache hit. We found the cache needed
      num_hits ++;
      memcpy(buf, currCacheIndex->block, JBOD_BLOCK_SIZE);
      clock ++;
      currCacheIndex->access_time = clock;
      return 1;
    }
    
    currCacheIndex ++;
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  if((cache == NULL) | (buf == NULL))
  {
    return;
  }
  
  if(cacheEmpty)
  {
    cache_insert(disk_num, block_num, buf);
    cacheEmpty = false;
    return;
  }
  
  cache_entry_t *currCacheIndex;
  currCacheIndex = cache;
  
  for(int i = 0; i < cache_size; i++)
  {
    if((currCacheIndex->disk_num == disk_num) & (currCacheIndex->block_num == block_num))
    {
      // update current cache with buf content
      memcpy(currCacheIndex->block, buf, JBOD_BLOCK_SIZE);
      cacheEmpty = false;
      clock ++;
      currCacheIndex->access_time = clock;
      return;
    }
    
    currCacheIndex ++;
  }
  
  cache_insert(disk_num, block_num, buf);
  return;
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if((disk_num < 0) | (disk_num > 15) | (block_num < 0) | (block_num > 255) | (buf == NULL) | (cache == NULL))
  {
    return -1;
  }
  
  // where we are in the cache
  cache_entry_t *currCacheIndex;
  currCacheIndex = cache;
  
  for(int i = 0; i < cache_size; i++)
  {
    // block already in cache
    if((currCacheIndex->disk_num == disk_num) & (currCacheIndex->block_num == block_num) & (currCacheIndex->valid == true))
    {
      cacheEmpty = false;
      clock ++;
      currCacheIndex->access_time = clock;
      memcpy(currCacheIndex->block, buf, JBOD_BLOCK_SIZE);
      return -1;
    }
    
    currCacheIndex ++;
  }
  
  currCacheIndex = cache;
  
  if(cacheFull)
  {
    // replace the least recently used index
    // aka the index with the smallest access_time
    int minTime = currCacheIndex->access_time;
    cache_entry_t *lruCache;
    lruCache = currCacheIndex;
    
    for(int i = 0; i < cache_size; i++)
    {
      if(minTime > currCacheIndex->access_time)
      {
        minTime = currCacheIndex->access_time;
        lruCache = currCacheIndex;
      }
      
      currCacheIndex ++;
    }
    
    currCacheIndex = cache;
    
    // replace the lru index
    clock ++;
    lruCache->access_time = clock;
    lruCache->disk_num = disk_num;
    lruCache->block_num = block_num;
    lruCache->valid = true;
    memcpy(lruCache->block, buf, JBOD_BLOCK_SIZE);
  }
  else
  {
    // if the cache is not full, linearly insert the entry.
    cacheEmpty = false;
    clock ++;
    
    currCache->access_time = clock;
    currCache->disk_num = disk_num;
    currCache->block_num = block_num;
    currCache->valid = true;
    memcpy(currCache->block, buf, JBOD_BLOCK_SIZE);
    numFilledCache ++;
    currCache ++;
    
    if(numFilledCache == cache_size)
    {
      cacheFull = true;
      // reset currCache to the beginning of cache.
      currCache = cache;
    }
  }

  return 1;
}

bool cache_enabled(void) {
  if(cache_size > 2)
  {
    return true;
  }
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
