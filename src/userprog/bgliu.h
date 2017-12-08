#ifndef USERPROG_BGLIU_H
#define USERPROG_BGLIU_H

struct file_info{
    int id;                     // id of file
    struct list_elem elem;      // list elem of file
    struct file *file_name;     // file name
};

#endif
