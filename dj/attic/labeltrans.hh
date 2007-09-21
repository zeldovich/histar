#ifndef JOS_INC_DIS_LABELTRANS_HH
#define JOS_INC_DIS_LABELTRANS_HH

#include <dj/globallabel.hh>


class label_trans
{
public:
    label_trans(struct cobj_ref catdir_sg);
    label_trans(struct catdir *dir);

    ~label_trans(void);

    void client_is(struct cobj_ref client_gt);

    void local_for(global_label *gl, label *l);
    void local_for(global_label *gl, label *l, 
		   global_to_local fn, void *arg);

    static int64_t get_local(global_cat *gcat, void *arg);
    void localize(const global_label *gl);
    
    
private:
    struct cobj_ref catdir_sg_;
    struct cobj_ref client_gt_;

    struct catdir *catdir_;

};

#endif
