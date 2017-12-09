#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "devices/timer.h"
#include "userprog/bgliu.h"
#include "userprog/umem.h"

//SYSTEM CALL FUNCTIONS

static bool sys_create(const char *file, unsigned size){
    lock_acquire(&lock);
    bool success = filesys_create(file, size, false);
    lock_release(&lock);
    return success;
}

void create_handler(struct intr_frame *f){
    const char *file;
    unsigned size;
    
    umem_read(f->esp + 4, &file, sizeof(file));     
    umem_read(f->esp + 8, &size, sizeof(size));
    
    f->eax = sys_create(file, size);
}

static int sys_open(char *filename){
    struct file_info *f = palloc_get_page(0);
    lock_acquire(&lock);
    struct file *file = filesys_open(filename);
    if (file == NULL){
        lock_release(&lock);
        return -1;
    }
    f->file_name = file;
    f->id = thread_current()->open_files + 3;

    thread_current()->open_files++;
    list_push_back(&thread_current()->file_list, &f->elem);
    lock_release(&lock);
    
    return f->id;
}

void open_handler(struct intr_frame *f){
    char *file;
    umem_read(f->esp + 4, &file, sizeof(file));
    f->eax = sys_open(file);
}

static struct file_info *find_file(int file_id){
    struct file_info *f = NULL;
    if (list_empty(&thread_current()->file_list)){
        lock_release(&lock);
    }
    else{
        for (struct list_elem *curr = list_front(&thread_current()->file_list); 
            curr != list_end(&thread_current()->file_list); curr = list_next(curr)){
            struct file_info *curr_file = list_entry(curr, struct file_info, elem);
            if (curr_file->id == file_id){
                f = curr_file;
                break;
            }
        }
        if (f == NULL){
            lock_release(&lock);
        }
    }
    return f;
}

static int sys_read(int file_id, void *buffer, int size){
    lock_acquire(&lock);
    struct file_info *f = find_file(file_id);
    if (f==NULL){
        return -1;
    }
    lock_release(&lock);
    return file_read(f->file_name, buffer, size);
}

void read_handler(struct intr_frame *f){
    int file_id;
    void *buffer;
    int size;
    
    umem_read(f->esp + 4, &file_id, sizeof(file_id));
    umem_read(f->esp + 8, &buffer, sizeof(buffer));
    umem_read(f->esp + 12, &size, sizeof(size));
    
    f->eax = sys_read(file_id, buffer, size);
}

static uint32_t sys_write(int fd, const void *buffer, unsigned size)
{
    umem_check((const uint8_t*) buffer);
    umem_check((const uint8_t*) buffer + size - 1);

    int ret = -1;
    
    if (fd == 1) { // write to stdout
      putbuf(buffer, size);
      ret = size;
    }
    lock_acquire(&lock);
    struct file_info *fi = NULL;
    if(!list_empty(&thread_current()->file_list)){
        for(struct list_elem *curr = list_front(&thread_current()->file_list); 
        curr != list_end(&thread_current()->file_list);
        curr = list_next(curr)){
            struct file_info *curr_info = list_entry(curr, struct file_info, elem);
            if(curr_info->id == fd){
                fi = curr_info;
                break;
            }
        }
        if(fi == NULL){
            lock_release(&lock);
            return -1;
        } 
    }else{
      lock_release(&lock);
      return -1;
  }

  ret = file_write(fi->file_name, buffer, size);
  lock_release(&lock);


  return (uint32_t) ret;
}

void write_handler(struct intr_frame *f)
{
    int fd;
    const void *buffer;
    unsigned size;

    umem_read(f->esp + 4, &fd, sizeof(fd));
    umem_read(f->esp + 8, &buffer, sizeof(buffer));
    umem_read(f->esp + 12, &size, sizeof(size));

    f->eax = sys_write(fd, buffer, size);
}

static int sys_file_size(int file_id){
    lock_acquire(&lock);
    struct file_info *f = find_file(file_id);
    int ret = file_length(f->file_name);
    lock_release(&lock);
    return ret;
}

void file_size_handler(struct intr_frame *f){
    int file_id;
    
    umem_read(f->esp + 4, &file_id, sizeof(file_id));
    
    f->eax = sys_file_size(file_id);
}

void sys_exit(int status) 
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

void exit_handler(struct intr_frame *f) 
{
  int exitcode;
  umem_read(f->esp + 4, &exitcode, sizeof(exitcode));

  sys_exit(exitcode);
}

static void sys_close(int file_id){
    lock_acquire(&lock);
    struct file_info *f = find_file(file_id);
    file_close(f->file_name);
    list_remove(&f->elem);
    lock_release(&lock);
}

void close_handler(struct intr_frame *f){
    int file_id;
    
    umem_read(f->esp + 4, &file_id, sizeof(file_id));
    
    sys_close(file_id);
}