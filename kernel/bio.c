// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13 // Upper limit of hash buckets
#define INTMAX 0x7fffffff

int hash(uint dev, uint blockno) {return ((blockno) % NBUCKET);} // Hash function

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Maintain NBUCKET buckets, each holding a linked list.
  // Each bucket has its own lock to protect its list.
  // The individual elements buf stored in each bucket
  // have their own locks, serving as buffer locks.
  struct buf bucket[NBUCKET];
  struct spinlock bucket_locks[NBUCKET];
} bcache;

void
binit(void)
{ 
  initlock(&bcache.lock, "bcache_lock");

    // Initialize buckets
    char name[32];
    for (int i = 0; i < NBUCKET; i++) {
        snprintf(name, 32, "bucket_lock_%d", i);
        initlock(&bcache.bucket_locks[i], name);
        bcache.bucket[i].next = 0;
    }

    // Initialize buffers
    for (int i = 0; i < NBUF; i++) {
        struct buf *b = &bcache.buf[i]; // Head pointer of the linked list
        initsleeplock(&b->lock, "buffer");

        b->lut = 0;
        b->refcnt = 0;
        b->curBucket = 0;

        // Insert buffer into bucket[0]
        b->next = bcache.bucket[0].next;
        bcache.bucket[0].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
    uint index = hash(dev, blockno);

    acquire(&bcache.bucket_locks[index]);
    struct buf *b = bcache.bucket[index].next;

    // Search within the current bucket[index]
    while (b) {
        if (b->dev == dev && b->blockno == blockno) {
            // Block found
            b->refcnt++;
            release(&bcache.bucket_locks[index]);
            acquiresleep(&b->lock);
            return b;
        }
        b = b->next;
    }

    // Not found in the current bucket, need to search in other buckets
    // It's not safe to acquire other bucket locks while holding one bucket_lock
    // Acquiring multiple bucket locks simultaneously can lead to deadlock
    // So, release the current bucket lock first
    release(&bcache.bucket_locks[index]);

    // As other processes might have moved the block to another bucket,
    // we need to check if the block is already in another bucket
    // If it's already in another bucket, wait for its sleeplock and return
    acquire(&bcache.lock);
    b = bcache.bucket[index].next;
    while (b) {
        if (b->dev == dev && b->blockno == blockno) {
            // Block has been moved to another bucket by another process
            acquire(&bcache.bucket_locks[index]);
            b->refcnt++;
            release(&bcache.bucket_locks[index]);
            release(&bcache.lock); // Release the bcache lock

            // Re-acquire the block's sleeplock and return after waiting
            acquiresleep(&b->lock);
            return b;
        }
        b = b->next;
    }

    // If the block is still not found, search for an empty buffer or LRU buffer
    struct buf *LRUb = 0;
    uint curBucket = -1;
    uint lut = INTMAX;

    for (int i = 0; i < NBUCKET; i++) {
        acquire(&bcache.bucket_locks[i]);

        b = &bcache.bucket[i];
        int found = 0;

        while (b->next) {
            if (b->next->refcnt == 0 && LRUb == 0) {
                // If an empty buffer is found and no empty buffer was found before
                LRUb = b;
                lut = b->next->lut;
                found = 1;
            }
            else if (b->next->refcnt == 0 && b->next->lut < lut) {
                // If an empty buffer is found and this buffer was used earlier
                LRUb = b;
                lut = b->next->lut;
                found = 1;
            }
            b = b->next;
        }
        if (found) {
            // LRUb has been updated, release the locks of the previously accessed buckets
            if (curBucket != -1) {
                // Release the lock of the previously accessed bucket
                release(&bcache.bucket_locks[curBucket]);
            }
            curBucket = i;
        }
        else {
            // If not found, release the lock of the accessed bucket
            release(&bcache.bucket_locks[i]);
        }
    }
    if (LRUb == 0) {
        panic("bget: No buffer.");
    }
    else {
        struct buf *p = LRUb->next;

        if (curBucket != index) {
            // Remove the LRUb node
            LRUb->next = p->next;
            release(&bcache.bucket_locks[curBucket]);

            // Insert the LRUb node into the current bucket[index]
            acquire(&bcache.bucket_locks[index]);
            p->next = bcache.bucket[index].next;
            bcache.bucket[index].next = p;
        }

        // Update information of LRUb
        p->dev = dev;
        p->blockno = blockno;
        p->refcnt = 1;
        p->valid = 0;
        p->curBucket = index;

        release(&bcache.bucket_locks[index]); // Release the bucket[index] lock
        release(&bcache.lock);                // Release the bcache lock
        acquiresleep(&p->lock);               // Acquire the sleeplock of LRUb
        return p;
    }
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint index = hash(b->dev, b->blockno);
    acquire(&bcache.bucket_locks[index]);
    b->refcnt--;
    if (b->refcnt == 0) 
        b->lut = ticks;
    release(&bcache.bucket_locks[index]);
}

void bpin(struct buf *b)
{

    uint index = hash(b->dev, b->blockno);
    acquire(&bcache.bucket_locks[index]);
    b->refcnt++;
    release(&bcache.bucket_locks[index]);
}

void bunpin(struct buf *b)
{

    uint index = hash(b->dev, b->blockno);
    acquire(&bcache.bucket_locks[index]);
    b->refcnt--;
    release(&bcache.bucket_locks[index]);
}

