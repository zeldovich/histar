#ifndef JOS_INC_DIRENT_H
#define JOS_INC_DIRENT_H

struct dirent {
    char d_name[KOBJ_NAME_LEN];
    struct fs_inode d_inode;
};

typedef struct {
    struct dirent dir_de;
} DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif
