#ifndef JOS_INC_GLOBAL_LABEL_HH_
#define JOS_INC_GLOBAL_LABEL_HH_

#include <inc/label.h>
#include <inc/fs.h>

class label;

#define GLOBAL_LEN 32

struct global_entry 
{
    char global[GLOBAL_LEN];
    level_t level;
} __attribute__((packed));

class global_label 
{
public:
    global_label(label *local);
    global_label(const char *serial);
    ~global_label(void) {
        delete entry_;
        if (serial_)
            delete serial_;    
        if (string_)
            delete string_;    
    }
    static global_label *global_for_obj(const char *path);
        
    const char *serial(void) const;
    int         serial_len(void) const;
    const char *string_rep(void) const;
            
private:       
    void gen_serial(void);

    global_entry *entry_;
    uint32_t entries_;
    level_t default_;
    char *serial_;
    char *string_;
    int serial_len_;
};

#endif /*JOS_INC_GLOBAL_LABEL_HH_*/
