#ifndef USERPROG_BGLIU_H
#define USERPROG_BGLIU_H

#include "threads/lock.h"

struct file_info{
    int id;                     // id of file
    struct list_elem elem;      // list elem of file
    struct file *file_name;     // file name
};

struct lock lock;

void create_handler(struct intr_frame *);
void open_handler(struct intr_frame *);
void read_handler(struct intr_frame *);
void file_size_handler(struct intr_frame *);
void write_handler(struct intr_frame *);
void exit_handler(struct intr_frame *);
void close_handler(struct intr_frame *);
#endif
