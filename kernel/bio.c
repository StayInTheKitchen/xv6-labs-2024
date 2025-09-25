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

#define NBUCKET 101

struct bcache_bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct bcache_bucket buckets[NBUCKET];
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

int
bhash(uint blockno)
{
  return blockno % NBUCKET;
}

void
blist_init(struct buf *head)
{
  head->next = head;
  head->prev = head;
}

void
blist_insert_front(struct buf *head, struct buf *b)
{
  b->next = head->next;
  b->prev = head;
  head->next->prev = b;
  head->next = b;
}

void
blist_remove(struct buf *b)
{
  b->prev->next = b->next;
  b->next->prev = b->prev;
  b->next = 0;
  b->prev = 0;
}

void
binit(void)
{
  initlock(&bcache.lock, "bcache");
	
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.buckets[i].lock, "bcache.bucket");
    blist_init(&bcache.buckets[i].head);
  }

  for (struct buf *b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->valid = 0;
    b->disk = 0;
    b->dev = 0;
    b->blockno = 0;
    b->refcnt = 0;
    b->prev = b->next = 0;
    initsleeplock(&b->lock, "bcache.buf");
  }
}

static struct buf*
bget(uint dev, uint blockno)
{
  int idx = bhash(blockno);
  struct bcache_bucket *bk = &bcache.buckets[idx];
  struct buf *b;

  acquire(&bk->lock);
  for (b = bk->head.next; b != &bk->head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bk->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bk->lock);

  acquire(&bcache.lock);

  struct buf *freeb = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    if (b->refcnt == 0) {
      freeb = b;
      break;
    }
  }

  if (freeb == 0)
    panic("bget: no buffers");

  freeb->refcnt = 1;
  release(&bcache.lock);

  freeb->dev = dev;
  freeb->blockno = blockno;
  freeb->valid = 0;

  acquire(&bk->lock);
  for (b = bk->head.next; b != &bk->head; b = b->next) {
    acquire(&bcache.lock);
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bk->lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bcache.lock);
  }

  blist_insert_front(&bk->head, freeb);
  release(&bk->lock);

  acquiresleep(&freeb->lock);
  return freeb;
}

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int idx = bhash(b->blockno);
  struct bcache_bucket *bk = &bcache.buckets[idx];

  acquire(&bk->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    if (b->prev && b->next)
      blist_remove(b);
  }
  release(&bk->lock);
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

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}