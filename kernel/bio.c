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

#define NBUCKETS 13

struct bucket {
  struct spinlock lock;
  struct buf head;
  char lock_name[16];
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKETS; ++i) {
    struct bucket *bkt = bcache.buckets + i;
    snprintf(bkt->lock_name, 15, "bcache.bucket%d", i);
    initlock(&bkt->lock, bkt->lock_name);
    bkt->head.prev = &bkt->head;
    bkt->head.next = &bkt->head;
  }

  struct buf *head = &bcache.buckets[0].head;
  head->prev = head;
  head->next = head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = head->next;
    b->prev = head;
    initsleeplock(&b->lock, "buffer");
    head->next->prev = b;
    head->next = b;
  }
}

static struct buf *find_unused_buffer(struct bucket *bkt) {
  for (struct buf *b = bkt->head.next; b != &bkt->head; b = b->next) {
    if (b->refcnt == 0) {
      return b;
    }
  }
  return 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  struct bucket *bkt = bcache.buckets + blockno % NBUCKETS;
  acquire(&bkt->lock);

  for (b = bkt->head.next; b != &bkt->head; b = b->next) {
    if (b->dev != dev || b->blockno != blockno) {
      continue;
    }
    b->refcnt++;
    release(&bkt->lock);
    acquiresleep(&b->lock);
    return b;
  }

  if ((b = find_unused_buffer(bkt)) != 0) {
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bkt->lock);
    acquiresleep(&b->lock);
    return b;
  }

  for (struct bucket *bkt0 = bcache.buckets; bkt0 < bcache.buckets + NBUCKETS;
       ++bkt0) {
    if (bkt0 == bkt) {
      continue;
    }

    acquire(&bkt0->lock);

    b = find_unused_buffer(bkt0);
    if (b == 0) {
      release(&bkt0->lock);
      continue;
    }

    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bkt->head.next;
    b->prev = &bkt->head;
    bkt->head.next->prev = b;
    bkt->head.next = b;

    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;

    release(&bkt0->lock);
    release(&bkt->lock);
    acquiresleep(&b->lock);

    return b;
  }
  panic("bget: no buffers");
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
  __sync_fetch_and_sub(&b->refcnt, 1);
}

void
bpin(struct buf *b) {
  __sync_fetch_and_add(&b->refcnt, 1);
}

void
bunpin(struct buf *b) {
  __sync_fetch_and_sub(&b->refcnt, 1);
}


