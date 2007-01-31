#ifndef JOS_INC_GLOBAL_LABEL_HH
#define JOS_INC_GLOBAL_LABEL_HH

#include <inc/label.h>
#include <inc/fs.h>

#include <dj/globalcat.h>

class label;
class global_label;

struct global_entry 
{
    struct global_cat global;
    level_t level;
} __attribute__((packed));

typedef void (global_converter)(uint64_t cat, global_cat *gcat);
typedef int64_t (global_to_local)(global_cat *gcat, void *arg);

class global_label 
{
public:
    global_label(uint64_t def) : 
	entry_(0), default_(def), serial_(0), string_(0), local_label_(0) {}
    global_label(struct ulabel *local, global_converter *get_global);
    global_label(const char *serial);
    ~global_label(void) { cleanup(); }
    
    static global_label *global_for_obj(const char *path);
        
    const char *serial(void) const;
    uint32_t    serial_len(void) const;
    void        serial_is(const char *serial);

    const label *local_label(global_to_local *fn, void *arg);

    const char *string_rep(void) const;
    
    const global_entry *entries(void) const { return entry_; }
    uint32_t            entries_count(void) const { return entries_; } 
    
private:       
    void copy_from(const char *serial);
    void cleanup(void);
    void gen_serial(void);
    void gen_local_label(global_to_local *g2l, void *arg);
    
    global_entry *entry_;
    uint32_t entries_;
    level_t default_;
  
    char *serial_;
    char *string_;
    label *local_label_;
    
    int serial_len_;
};

#endif /*JOS_INC_GLOBAL_LABEL_HH_*/
