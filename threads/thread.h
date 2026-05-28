#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include "threads/synch.h"
#include <debug.h>
#include <list.h>
#include <stdint.h>

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A shared box between parent and child to track status safely. */
struct child_status {
    tid_t tid;                          
    int exit_status;                    
    bool is_alive;                      
    bool was_waited_on;                 
    struct semaphore wait_sema;         
    
    /* ADD THESE TWO LINES: */
    struct semaphore load_sema;         /* Parent sleeps here during exec() */
    bool load_success;                  /* Did the executable load successfully? */
    
    struct list_elem elem;              
};
struct thread
  {
    /* Project 2: Process Guardian */
    int exit_status;                        /* The exit status of this thread */
    struct list children;                   /* List of child_status structs */
    struct child_status *my_status;         /* The child's pointer to its own box */ 

    /* Project 2: File System I/O */
    int fd_count;                           /* Next available File Descriptor */
    struct file *fd_table[128];             /* Array to track open files */
    struct file *exec_file;                 /* Read-Only Executable Lock */

    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

struct thread *thread_get_by_tid (tid_t tid);

#endif /* threads/thread.h */