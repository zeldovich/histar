#ifndef JOS_HTTP_INC_H
#define JOS_HTTP_INC_H

struct arg_desc {
    const char *name;
    char val[32];
};

int argv_parse(int ac, const char **av, struct arg_desc *adv);
char *arg_val(struct arg_desc *adv, const char *name);


#endif
