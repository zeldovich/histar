#ifndef JOS_INC_PWD_H
#define JOS_INC_PWD_H

struct passwd {
    char *pw_dir;
};

struct passwd *getpwnam(const char *name);

#endif
