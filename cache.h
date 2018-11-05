/**************************************************************/
/* CS/COE 1541
   Project 2 5-Stage Pipeline Simulator with Cache simulation
   Collaborators: Zachary Whitney, Albert Yang, Jeremy Kato
   IDS: zdw9, aly31, jdk81
***************************************************************/
/* This file contains a functional simulator of an associative cache with LRU replacement*/
#include <stdlib.h>
#include <stdio.h>

struct cache_blk_t { // note that no actual data will be stored in the cache
  unsigned long tag;
  unsigned int address; //the block address
  char valid;
  char dirty;
  unsigned LRU;	//to be used to build the LRU stack for the blocks in a cache set
};

struct cache_t {
	// The cache is represented by a 2-D array of blocks.
	// The first dimension of the 2D array is "nsets" which is the number of sets (entries)
	// The second dimension is "assoc", which is the number of blocks in each set.
  int nsets;					// number of sets
  int blocksize;				// block size
  int assoc;					// associativity
  int miss_penalty;				// the miss penalty
  struct cache_blk_t **blocks;	// a pointer to the array of cache blocks
};

struct cache_t * cache_create(int size, int blocksize, int assoc)
{
  int i, nblocks , nsets ;
  struct cache_t *C = (struct cache_t *)calloc(1, sizeof(struct cache_t));

  nblocks = size *1024 / blocksize ;// number of blocks in the cache
  nsets = nblocks / assoc ;			// number of sets (entries) in the cache
  C->blocksize = blocksize ;
  C->nsets = nsets  ;
  C->assoc = assoc;

  C->blocks= (struct cache_blk_t **)calloc(nsets, sizeof(struct cache_blk_t *));

  for(i = 0; i < nsets; i++) {
		C->blocks[i] = (struct cache_blk_t *)calloc(assoc, sizeof(struct cache_blk_t));
	}
  return C;
}
//------------------------------

int updateLRU(struct cache_t *cp ,int index, int way)
{
int k ;
for (k=0 ; k< cp->assoc ; k++)
{
  if(cp->blocks[index][k].LRU < cp->blocks[index][way].LRU)
     cp->blocks[index][k].LRU = cp->blocks[index][k].LRU + 1 ;
}
cp->blocks[index][way].LRU = 0 ;
}


int cache_access(struct cache_t *cp, unsigned long address, int access_type, 
  int *evict_to_return /*0 for read, 1 for write*/)
//returns 0 (if a hit), 1 (if a miss but no dirty block is writen back) or
//2 (if a miss and a dirty block is writen back)
//if a dirty block is written back, set evict_to_return to its address
{
int i ;
int block_address ;
int index ;
int tag ;
int way ;
int max ;

if(!cp->nsets)
   return 0;
block_address = (address / cp->blocksize);
tag = block_address / cp->nsets;
index = block_address - (tag * cp->nsets) ;

for (i = 0; i < cp->assoc; i++) {	/* look for the requested block */
  if (cp->blocks[index][i].tag == tag && cp->blocks[index][i].valid == 1)
  {
	updateLRU(cp, index, i) ;
	if (access_type == 1) cp->blocks[index][i].dirty = 1 ;
	return(0);						/* a cache hit */
  }
}
/* a cache miss */
for (way=0 ; way< cp->assoc ; way++)		/* look for an invalid entry */
    if (cp->blocks[index][way].valid == 0)	/* found an invalid entry */
	{
      cp->blocks[index][way].valid = 1 ;
      cp->blocks[index][way].tag = tag ;
      cp->blocks[index][way].LRU = way ;
      cp->blocks[index][way].address = block_address;
	  updateLRU(cp, index, way);
	  cp->blocks[index][way].dirty = 0;
      if(access_type == 1) cp->blocks[index][way].dirty = 1 ;
	  return(1);
    }

 max = cp->blocks[index][0].LRU ;			/* find the LRU block */
 way = 0 ;
 for (i=1 ; i< cp->assoc ; i++)
  if (cp->blocks[index][i].LRU > max) {
    max = cp->blocks[index][i].LRU ;
    way = i ;
  }
// cp->blocks[index][way] is the LRU block
*evict_to_return = (cp->blocks[index][way].address); //return evicted block's address for the WB
cp->blocks[index][way].tag = tag;
cp->blocks[index][way].address = block_address;
updateLRU(cp, index, way);
if (cp->blocks[index][way].dirty == 0)
  {											/* the evicted block is clean*/
	cp->blocks[index][way].dirty = access_type;
	return(1);
  }
else
  {											/* the evicted block is dirty*/
	cp->blocks[index][way].dirty = access_type;
	return(2);
  }

}

