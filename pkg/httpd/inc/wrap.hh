#ifndef JOS_HTTPD_INC_WRAP_HH
#define JOS_HTTPD_INC_WRAP_HH

#include <iostream>
#include <sstream>
#include <inc/cpplabel.hh>

class wrap_call {
public:
    wrap_call(const char *pn, fs_inode root_ino);
    ~wrap_call(void);

    void call(int ac, const char **av, int ec, const char **ev, 
	      label *taint_label);
    void call(int ac, const char **av, int ec, const char **ev, 
	      label *taint_label , std::ostringstream &out);

    void pipe(wrap_call *wc ,int ac, const char **av, 
	      int ec, const char **ev, label *taint_label);
    void pipe(wrap_call *wc ,int ac, const char **av, int ec, const char **ev, 
	      label *taint_label, std::ostringstream &out);

    const child_process *child_proc(void) { return &cp_; }

    int sin_;   // defaults to /dev/null
    int eout_;  // defaults to /dev/null
    
private:
    static void print_to(int fd, std::ostringstream &out);

    int sout_;  // pipe[1]
    int wout_;  // pipe[0]

    fs_inode root_ino_;

    char *pn_;
   
    cobj_ref call_ct_;
    child_process cp_;    
    char called_;
};

#endif
