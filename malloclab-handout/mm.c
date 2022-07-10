/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4         /* 字长，头部或者底部长度，单位byte*/

#define DSIZE 8         // 双字

#define CHUNKSIZE (1 << 12)  //通过这个量扩展堆

#define MAX(x,y) ((x) > (y)?(x):(y))

#define PACK(size,alloc) ((size)|(alloc)) //将块大小和分配符结合

#define GET(p) (*(unsigned int*)(p))  //在p读一个字

#define PUT(p,val) (*(unsigned int*)(p) = (val)) //在p写一个字

#define GET_SIZE(p) (GET(p) & ~0x7) //获取分配区的大小

#define GET_ALLOC(p) (GET(p) & 0x1) //获取分配状态

#define HDRP(bp) ((char *)(bp) - WSIZE) //bp为栈底指针，减去一个字为顶地址

#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE )//通过bp加上整个分配大小，减去两个字，为底部地址
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp))) //以下两个操作都是到达另外一个块的bp处。  因为头部 脚步存储的长度为 所有组成部分长度

#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*) (bp)-DSIZE )))

static void *extend_heap(size_t words);
static void *coalesce(void *ptr);
static void *find_fit(size_t asize);
static void place(void *ptr, size_t asize);
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
int mm_check(void);
static char* heap_listp = 0;


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //1.分配初始空间，对齐，序言，尾部
    //
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*) -1)
        return -1;
    PUT(heap_listp,0);
    PUT(heap_listp+(1 * WSIZE),PACK(DSIZE,1));
    PUT(heap_listp+(2*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp+(3*WSIZE),PACK(0,1));
    heap_listp+= (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void* extend_heap(size_t words)
{
   char* bp;
   //1.因为要保持对齐，所以校验纠正更新 大小为偶数，
    size_t  change_size = (words%2) ? (words+1)*WSIZE : words*WSIZE;
    
    //2.通过调用sbrk扩容，sbrk返回的是新空闲区域的起始地址。之前尾部的header的下一个字为开始位置，把视为bp，之前的尾部header变为新的空闲头header，新增区域尾部字改为尾部header；
    
    if ((long)(bp = mem_sbrk(change_size)) ==  -1 )
        return NULL;
    PUT(HDRP(bp),PACK(change_size,0));
    PUT(FTRP(bp),PACK(change_size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    //3.因为新分配后，前一个堆是空闲块，所以需要进行一次合并。
    return coalesce(bp);
}

static void* coalesce(void* bp)
{
    //四种情况 所以先获取前一个与下一个块状态
    //
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    //1.前不空，后不空
    if (prev_alloc && next_alloc)
        return bp;
    //2.前不空，后控
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }

     else if (!prev_alloc && next_alloc) { /* Case 3 */
        //前面一块空闲，后面一块不空闲
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));  //更新新空闲块大小
        PUT(FTRP(bp), PACK(size, 0));  //修改此块的脚部，作为两块的脚部
        PUT(HDRP(PREV_BLKP(bp)),PACK(size, 0));  //顺着本块未更新的头部信息，找到前块的头部，对其进行修改
        bp = PREV_BLKP(bp);  //并将返回的新空闲块指针修改为前块的
    }

    else { /* Case 4 */
        //前后块都空闲
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  //更新空闲块大小
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));  //修改前块的头部，作为三块的头部
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));  //修改后块的脚部，作为三块的脚部
        bp = PREV_BLKP(bp);  //并将返回的新空闲块指针修改为前块的
    }
    return bp; 
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* Adjust block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *ptr;

    /* Ignore spurious requests */
    if(size == 0){
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    if(size <= DSIZE){
        asize = 2 * DSIZE; //至少16字节
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); //向上与8字节对齐
    }

    /* Search the free list for a fit */
    if((ptr = find_fit(asize)) != NULL) {
        place(ptr, asize);
        return ptr;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if((ptr = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(ptr, asize);
    return ptr;
}

static void* find_fit(size_t size)
{
    void* res = NULL;
    void* ptr;
    size_t min_size = 0x3fffffff;
    for (ptr = heap_listp;GET_SIZE(HDRP(ptr))>0;ptr = NEXT_BLKP(ptr))
    {
       if (GET_ALLOC(HDRP(ptr)))
           continue;
        size_t block_size = GET_SIZE(HDRP(ptr));
        if (block_size >= size && block_size <min_size){
            min_size = block_size;
            res = ptr;
        }
    }
    return res;
}

static void place(void* ptr,size_t asize){
    //修改头部大小，增加头部，脚步。
    //1.取出当前头部大小，算出新头部大小
    //2.修改当前头部大小，跳到新脚设置。
    //3.从旧脚步修改，跳到新头修改。
    //
    size_t block_size = GET_SIZE(HDRP(ptr));
    //判断是否需要截断
    if ((block_size-asize)>=(2*DSIZE)){
               PUT(HDRP(ptr), PACK(asize, 1)); //修改头部
        PUT(FTRP(ptr), PACK(asize, 1)); //顺着刚修改的头部，找到前块已分配块的脚部修改
        ptr = NEXT_BLKP(ptr); //顺着修改后的头部，找到后块未分配块的有效载荷
        PUT(HDRP(ptr), PACK(block_size - asize, 0)); //修改后块未分配块的头部尾部，大小为差值
        PUT(FTRP(ptr), PACK(block_size - asize, 0)); 

    } else{
        PUT(HDRP(ptr),PACK(block_size,1));
        PUT(FTRP(ptr),PACK(block_size,1));
    }
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    //释放一个块 然后合并它前后块
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
        size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if (!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr)) - DSIZE;
    if (size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}















