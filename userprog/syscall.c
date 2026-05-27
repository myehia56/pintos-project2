#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"

struct lock filesys_lock;

/* Bouncer 1: Checks full buffers byte-by-byte */
static void check_valid_buffer (const void *buffer, unsigned size) {
  const uint8_t *ptr = (const uint8_t *)buffer;
  for (unsigned i = 0; i < size; i++) {
    if (ptr + i == NULL || !is_user_vaddr(ptr + i) || pagedir_get_page(thread_current()->pagedir, ptr + i) == NULL) {
        thread_current()->exit_status = -1;
        printf("%s: exit(-1)\n", thread_current()->name);
        thread_exit();
    }
  }
}

/* Bouncer 2: Checks strings until the null-terminator */
static void check_valid_string (const void *str) {
  if (str == NULL) {
      thread_current()->exit_status = -1;
      printf("%s: exit(-1)\n", thread_current()->name);
      thread_exit();
  }
  const char *ptr = (const char *)str;
  while (true) {
      if (!is_user_vaddr(ptr) || pagedir_get_page(thread_current()->pagedir, ptr) == NULL) {
          thread_current()->exit_status = -1;
          printf("%s: exit(-1)\n", thread_current()->name);
          thread_exit();
      }
      if (*ptr == '\0') break;
      ptr++;
  }
}

static void syscall_handler (struct intr_frame *);

void syscall_init (void) {
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f) {
  uint32_t *user_esp = f->esp;
  
  // Verify the system call number itself is safe to read
  check_valid_buffer(user_esp, 4); 

  int call = user_esp[0];
  struct thread *t = thread_current();

  switch (call) 
  {
    case SYS_HALT:
      shutdown_power_off();
      break;

    case SYS_EXIT:
      check_valid_buffer (user_esp, 8);
      t->exit_status = user_esp[1];
      printf("%s: exit(%d)\n", t->name, t->exit_status);
      thread_exit();
      break;

    case SYS_EXEC:
      check_valid_buffer (user_esp, 8);
      check_valid_string ((const char *)user_esp[1]);
      f->eax = process_execute((const char *)user_esp[1]);
      break;

    case SYS_WAIT:
      check_valid_buffer (user_esp, 8);
      f->eax = process_wait((tid_t)user_esp[1]);
      break;

    case SYS_CREATE:
      check_valid_buffer (user_esp, 12);
      check_valid_string ((const char *)user_esp[1]);
      lock_acquire(&filesys_lock);
      f->eax = filesys_create((const char *)user_esp[1], (unsigned)user_esp[2]);
      lock_release(&filesys_lock);
      break;

    case SYS_REMOVE:
      check_valid_buffer (user_esp, 8);
      check_valid_string ((const char *)user_esp[1]);
      lock_acquire(&filesys_lock);
      f->eax = filesys_remove((const char *)user_esp[1]);
      lock_release(&filesys_lock);
      break;

    case SYS_OPEN:
      check_valid_buffer (user_esp, 8);
      check_valid_string ((const char *)user_esp[1]);
      lock_acquire(&filesys_lock);
      struct file *file_obj = filesys_open((const char *)user_esp[1]);
      if (file_obj == NULL) {
          f->eax = -1;
      } else {
          int fd = t->fd_count++;
          t->fd_table[fd] = file_obj;
          f->eax = fd;
      }
      lock_release(&filesys_lock);
      break;

    case SYS_FILESIZE:
      check_valid_buffer (user_esp, 8);
      lock_acquire(&filesys_lock);
      if (user_esp[1] >= 2 && user_esp[1] < 128 && t->fd_table[user_esp[1]] != NULL) {
          f->eax = file_length(t->fd_table[user_esp[1]]);
      } else {
          f->eax = -1;
      }
      lock_release(&filesys_lock);
      break;

    case SYS_READ:
      check_valid_buffer (user_esp, 16);
      check_valid_buffer ((const void *)user_esp[2], (unsigned)user_esp[3]);
      
      if (user_esp[1] == 0) { 
          uint8_t *buf = (uint8_t *)user_esp[2];
          for (unsigned i = 0; i < user_esp[3]; i++) {
              buf[i] = input_getc();
          }
          f->eax = user_esp[3];
      } else if (user_esp[1] >= 2 && user_esp[1] < 128 && t->fd_table[user_esp[1]] != NULL) {
          lock_acquire(&filesys_lock);
          f->eax = file_read(t->fd_table[user_esp[1]], (void *)user_esp[2], (unsigned)user_esp[3]);
          lock_release(&filesys_lock);
      } else {
          f->eax = -1;
      }
      break;

    case SYS_WRITE:
      check_valid_buffer (user_esp, 16);
      check_valid_buffer ((const void *)user_esp[2], (unsigned)user_esp[3]);
      
      if (user_esp[1] == 1) { 
          putbuf((const char *)user_esp[2], (unsigned)user_esp[3]);
          f->eax = user_esp[3];
      } else if (user_esp[1] >= 2 && user_esp[1] < 128 && t->fd_table[user_esp[1]] != NULL) {
          lock_acquire(&filesys_lock);
          f->eax = file_write(t->fd_table[user_esp[1]], (const void *)user_esp[2], (unsigned)user_esp[3]);
          lock_release(&filesys_lock);
      } else {
          f->eax = -1;
      }
      break;

    case SYS_SEEK:
      check_valid_buffer (user_esp, 12);
      lock_acquire(&filesys_lock);
      if (user_esp[1] >= 2 && user_esp[1] < 128 && t->fd_table[user_esp[1]] != NULL) {
          file_seek(t->fd_table[user_esp[1]], (unsigned)user_esp[2]);
      }
      lock_release(&filesys_lock);
      break;

    case SYS_TELL:
      check_valid_buffer (user_esp, 8);
      lock_acquire(&filesys_lock);
      if (user_esp[1] >= 2 && user_esp[1] < 128 && t->fd_table[user_esp[1]] != NULL) {
          f->eax = file_tell(t->fd_table[user_esp[1]]);
      } else {
          f->eax = -1;
      }
      lock_release(&filesys_lock);
      break;

    case SYS_CLOSE:
      check_valid_buffer (user_esp, 8);
      lock_acquire(&filesys_lock);
      if (user_esp[1] >= 2 && user_esp[1] < 128 && t->fd_table[user_esp[1]] != NULL) {
          file_close(t->fd_table[user_esp[1]]);
          t->fd_table[user_esp[1]] = NULL;
      }
      lock_release(&filesys_lock);
      break;

    default:
      thread_exit();
      break;
  }
}