
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
typedef char ALIGN[16]; // Ensures memory is aligned to 16 bytes.

union header
{
    struct
    {
        size_t size;        // Size of the memory block.
        unsigned is_free;   // Whether the block is free (1 = free, 0 = used).
        union header *next; // Pointer to the next memory block.
    } s;
    ALIGN stub; // Ensures 16-byte alignment.
};
typedef union header header_t;

header_t *head = NULL, *tail = NULL; // Head and tail of the memory block linked list.
pthread_mutex_t global_malloc_lock;  // Mutex for thread safety.

header_t *get_free_block(size_t size)
{
    header_t *curr = head;
    while (curr)
    {
        if (curr->s.is_free && curr->s.size >= size)
        {
            return curr; // Found a suitable block.
        }
        curr = curr->s.next;
    }
    return NULL; // No suitable block found.
}

void *malloc(size_t size)
{
    size_t total_size;
    void *block;
    header_t *header;
    if (!size)
        return NULL; // Ignore zero-size requests.

    pthread_mutex_lock(&global_malloc_lock); // Lock for thread safety.

    header = get_free_block(size);
    if (header)
    { // Found a free block.
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)(header + 1); // Return memory after the header.
    }

    total_size = sizeof(header_t) + size; // Memory block size with header.
    block = sbrk(total_size);             // Request memory from OS.
    if (block == (void *)-1)
    {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL; // Allocation failed.
    }

    // Initialize the new block.
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;

    if (!head)
        head = header; // Update head if this is the first block.
    if (tail)
        tail->s.next = header; // Link to the previous block.
    tail = header;

    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(header + 1);
}

void free(void *block)
{
    header_t *header, *tmp;
    void *programbreak;

    if (!block)
        return;

    pthread_mutex_lock(&global_malloc_lock);

    header = (header_t *)block - 1; // Retrieve the block's header.
    programbreak = sbrk(0);         // Get the current program break.

    if ((char *)block + header->s.size == programbreak)
    {
        // If the block is the last one, shrink the program break.
        if (head == tail)
        {
            head = tail = NULL;
        }
        else
        {
            tmp = head;
            while (tmp)
            {
                if (tmp->s.next == tail)
                {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        sbrk(0 - header->s.size - sizeof(header_t)); // Release memory.
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }

    header->s.is_free = 1; // Mark the block as free.
    pthread_mutex_unlock(&global_malloc_lock);
}
void *calloc(size_t num, size_t nsize)
{
    size_t size = num * nsize;
    if (nsize != size / num)
        return NULL; // Check for overflow.

    void *block = malloc(size);
    if (!block)
        return NULL;

    memset(block, 0, size); // Zero-initialize memory.
    return block;
}
void *realloc(void *block, size_t size)
{
    header_t *header;
    void *ret;

    if (!block)
        return malloc(size); // Allocate if block is NULL.
    if (!size)
    {
        free(block); // Free if size is 0.
        return NULL;
    }

    header = (header_t *)block - 1;
    if (header->s.size >= size)
        return block; // No resizing needed.

    ret = malloc(size); // Allocate a new block.
    if (ret)
    {
        memcpy(ret, block, header->s.size); // Copy old data to new block.
        free(block);                        // Free the old block.
    }
    return ret;
}
void print_mem_list()
{
    header_t *curr = head;
    printf("head = %p, tail = %p \n", (void *)head, (void *)tail);
    while (curr)
    {
        printf("addr = %p, size = %zu, is_free = %u, next = %p\n",
               (void *)curr, curr->s.size, curr->s.is_free, (void *)curr->s.next);
        curr = curr->s.next;
    }
}
