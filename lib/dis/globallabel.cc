extern "C" {
#include <inc/fs.h>
#include <netinet/in.h>
#include <stdio.h>    
#include <string.h>
#include <assert.h>
}

#include <inc/dis/globallabel.hh>
//#include <inc/dis/globalcatc.hh>
#include <inc/dis/catc.hh>

#include <inc/labelutil.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>

static void 
get_global(uint64_t h, global_cat *global)
{
    uint64_t k;
    catc cc;
    if (cc.owns(h, &k)) {
        global->k = k;
        global->original = h;    
    }
    else
        throw basic_exception("cannot convert %ld", h);
}

static char
level_to_char(level_t lv)
{
    char lbuf[4];
    if (lv == LB_LEVEL_STAR)
        snprintf(&lbuf[0], sizeof(lbuf), "*");
    else
        snprintf(&lbuf[0], sizeof(lbuf), "%d", lv);
    return lbuf[0];
}

global_label::global_label(label *local) : serial_(0), string_(0)
{
    ulabel *ul = local->to_ulabel();
    entries_ = ul->ul_nent;
    entry_ = new global_entry[entries_];
    default_ = local->get_default();
    memset(entry_, 0, sizeof(global_entry) * entries_);
    try {
        for (uint64_t i = 0; i < ul->ul_nent; i++) {
            uint64_t h = LB_HANDLE(ul->ul_ent[i]);
            level_t l = LB_LEVEL(ul->ul_ent[i]);
            get_global(h, &entry_[i].global);
            entry_[i].level = l;
        }
    }
    catch (basic_exception e) {
        delete entry_;
        throw e;    
    }
}

global_label::global_label(const char *serial) : serial_(0), string_(0)
{
    entries_ = ntohl(((uint32_t *)serial)[0]);
    default_ = serial[4];
    
    entry_ = new global_entry[entries_];
    memset(entry_, 0, sizeof(global_entry) * entries_);
    
    uint32_t off = 5;
    memcpy(entry_, &serial[off], entries_ * sizeof(global_entry));
}

const char *
global_label::serial(void) const 
{
    if (!serial_) {
        global_label *me = (global_label *)this;
        me->gen_serial();
    }
    return serial_;
}
    
int         
global_label::serial_len(void) const
{
    if (!serial_) {
        global_label *me = (global_label *)this;
        me->gen_serial();
    }
    return serial_len_;    
}

void
global_label::gen_serial(void)
{
    static const uint32_t  bufsize = 256;
    char buf[bufsize];

    assert(bufsize >= sizeof(global_entry) * entries_ + 8);

    ((uint32_t*)buf)[0] = htonl(entries_);
    buf[4] = default_;

    uint32_t off = 5;
    memcpy(&buf[off], entry_, entries_ * sizeof(global_entry));
    off += entries_ * sizeof(global_entry);
    
    serial_ = new char[off];
    memcpy(serial_, buf, off);
    serial_len_ = off;
}

const char *
global_label::string_rep(void) const 
{
    if (string_)
        return string_;
    
    static const int bufsize = 256;
    char buf[bufsize];

    uint32_t off = 0;
    off += snprintf(&buf[off], bufsize - off, "{ ");
    for (uint32_t i = 0; i < entries_; i++) {
        level_t lv = entry_[i].level;
        if (lv == default_)
            continue;
        off += snprintf(&buf[off], bufsize - off, "(%ld, %ld):%c ",
                entry_[i].global.k, entry_[i].global.original, level_to_char(lv));
    }
    off += snprintf(&buf[off], bufsize - off, "%c }",
            level_to_char(default_));

    global_label *me = (global_label *)this;
    me->string_ = new char[strlen(buf) + 1];
    strcpy(string_, buf);
    return string_;
}

global_label *
global_label::global_for_obj(const char *path)
{
    fs_inode ino;
    error_check(fs_namei(path, &ino));
    label seg_label;
    obj_get_label(ino.obj, &seg_label);
    
    return new global_label(&seg_label);  
}

