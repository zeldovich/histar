#ifndef JOS_MACHINE_ACTOR_H
#define JOS_MACHINE_ACTOR_H

struct actor {
    uint64_t thread_id;
    uint64_t scratch_ct;
};

struct action {
    int type;
};

struct action_result {
    int64_t rval;
};

void actor_init(void);
void actor_create(struct actor *ar, int tainted);
void action_run(struct actor *ar, struct action *an, struct action_result *r);

enum {
    actor_action_noop,
    actor_action_create_segment,
    actor_action_max
};

#endif
