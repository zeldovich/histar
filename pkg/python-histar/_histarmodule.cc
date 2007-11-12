extern "C" {
    #include <Python.h>

    #include <inc/syscall.h>
    #include <inc/lib.h>
    #include <inc/label.h>
    #include <inc/labelutil.h>

    // For debugging
    #include <inc/stdio.h>
}
#include <inc/labelutil.hh>

/* static PyObject * */
/* histar_handle_create(PyObject *self, PyObject *args) */
/* { */
/*     const char *command; */
/*     int sts; */

/*     if (!PyArg_ParseTuple(args, "s", &command)) */
/*         return NULL; */
/*     sts = system(command); */
/*     return Py_BuildValue("i", sts); */
/* } */

/* Initialized in inithistar */
static PyObject *HiStarError;


typedef struct {
    PyObject_HEAD
    label *lbl;
} histar_Label;

static PyObject *
histar_Label_getitem(histar_Label *self, PyObject *args)
{
    uint64_t handle;

    cprintf("histar_Label_getitem\n");

    if (!PyArg_ParseTuple(args, "l", &handle))
        PyErr_SetString((PyObject *)HiStarError, "Requires a handle");
 
    return Py_BuildValue("b", self->lbl->get(handle));
}

static PyObject *
histar_Label_setitem(histar_Label *self, PyObject *args)
{
    uint64_t handle;
    level_t lvl;

    cprintf("histar_Label_setitem\n");

    if (!PyArg_ParseTuple(args, "lb", &handle, &lvl))
        PyErr_SetString((PyObject *)HiStarError, "Requires a handle and level");

    self->lbl->set(handle, lvl);

    Py_RETURN_NONE;
}

static PyObject *
histar_Label_get_default(histar_Label *self, PyObject *args)
{
    return Py_BuildValue("l", self->lbl->get_default());
}

static PyObject *
histar_Label_reset(histar_Label *self, PyObject *args)
{
    level_t lvl;

    if (!PyArg_ParseTuple(args, "b", &lvl))
        PyErr_SetString((PyObject *)HiStarError, "Requires a level");

    self->lbl->reset(lvl);

    Py_RETURN_NONE;
}

static PyObject *
histar_Label_from_string(histar_Label *self, PyObject *args)
{
    const char *str;

    if (!PyArg_ParseTuple(args, "s", &str))
        PyErr_SetString((PyObject *)HiStarError, "Requires a string");

    try {
        self->lbl->from_string(str);
    } catch (...) {
        PyErr_SetString((PyObject *)HiStarError, "Bad label string");
    }

    Py_RETURN_NONE;
}

static PyObject *
histar_Label_to_string(histar_Label *self, PyObject *args)
{
    return PyString_FromFormat("%s", self->lbl->to_string());
}

PyObject *histar_Label_alloc(PyTypeObject *type, Py_ssize_t size)
{
    histar_Label *self;
    cprintf("histar_Label_alloc: start");
    self = (histar_Label *)PyType_GenericAlloc(type, size);
    self->lbl = new label();
    cprintf("histar_Label_alloc: finished\n");
    return (PyObject *)self;
}

static void
histar_Label_dealloc(histar_Label *self)
{
    cprintf("histar_Label_dealloc: %s\n", self->lbl->to_string());
    delete self->lbl;
    self->ob_type->tp_free((PyObject*)self);
}


static PyMethodDef histar_LabelType_methods[] = {
    {"__getitem__", (PyCFunction)histar_Label_getitem, METH_VARARGS,
     "Return the level for a given category"},
    {"__setitem__", (PyCFunction)histar_Label_setitem, METH_VARARGS,
     "Set the level for a given category"},
    {"get_default", (PyCFunction)histar_Label_get_default, METH_VARARGS, ""},
    {"reset", (PyCFunction)histar_Label_reset, METH_VARARGS, ""},
    {"from_string", (PyCFunction)histar_Label_from_string, METH_VARARGS, ""},
    {NULL}  /* Sentinel */
};

static PyTypeObject histar_LabelType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "histar.Label",            /*tp_name*/
    sizeof(histar_Label),      /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)histar_Label_dealloc, /*tp_dealloc*/ 
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc)histar_Label_to_string, /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    (reprfunc)histar_Label_to_string, /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "HiStar Label",            /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    histar_LabelType_methods,      /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0/*(initproc)histar_Label_init*/,      /* tp_init */
    (allocfunc)histar_Label_alloc,                         /* tp_alloc */
    0/*histar_Label_new*/,          /* tp_new */
};






static PyObject *
histar_handle_create(PyObject *self, PyObject *args)
{
    uint64_t handle;
    /* Could use sys_handle_create here but the userspace equiv has caching */
    handle = handle_alloc();
    return Py_BuildValue("l", handle);
}

static PyObject *
histar_self_get_id(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", sys_self_id());
}

static PyObject *
histar_self_get_label(PyObject *self, PyObject *args)
{
    histar_Label *pylbl;

    cprintf("histar_self_get_label: start\n");
    pylbl = (histar_Label *)PyType_GenericNew((PyTypeObject *)&histar_LabelType,
                                              NULL, NULL);
    cprintf("histar_self_get_label: created new histar_Label\n");
    thread_cur_label(pylbl->lbl);
    cprintf("histar_self_get_label: got label from OS\n");

    return (PyObject *)pylbl;
}

static PyObject *
histar_self_get_clearance(PyObject *self, PyObject *args)
{
    histar_Label *pylbl;

    pylbl = (histar_Label *)PyType_GenericNew((PyTypeObject *)&histar_LabelType,
                                              NULL, NULL);

    thread_cur_clearance(pylbl->lbl);

    return (PyObject *)pylbl;
}


static PyObject *
histar_fs_create(PyObject *self, PyObject *args)
{
    struct fs_inode dir;
    struct fs_inode finode;

    const char *fn;
    const char *dirname;
    histar_Label *pylbl;

    if (!PyArg_ParseTuple(args, "ssO", &dirname, &fn, &pylbl))
        PyErr_SetString((PyObject *)HiStarError, "Requires a string directory,"
                        " filename, and histar.Label");

    cprintf("histar_fs_create: %s %s %s\n", dirname, fn,
            pylbl->lbl->to_string());

    if (fs_namei(dirname, &dir) < 0)
        PyErr_SetString((PyObject *)HiStarError, "Couldn't get inode for "
                        "specified directory");

    if (fs_create(dir, fn, &finode, pylbl->lbl->to_ulabel()) < 0)
        PyErr_SetString((PyObject *)HiStarError, "Couldn't create file");

    Py_RETURN_NONE;
}


static PyObject *
histar_fs_path_label(PyObject *self, PyObject *args)
{
    struct fs_inode dir;
    struct fs_inode finode;

    const char *pn;
    histar_Label *pylbl;

    if (!PyArg_ParseTuple(args, "s", &pn))
        PyErr_SetString((PyObject *)HiStarError, "Requires a pathname");

    pylbl = (histar_Label *)PyType_GenericNew((PyTypeObject *)&histar_LabelType,
                                              NULL, NULL);

    cprintf("histar_fs_path_label: %\n", pn);

    // TODO: get label from pn 
}


static PyMethodDef HiStarMethods[] = {
    {"create_category",  histar_handle_create, METH_VARARGS,
     "Create a new category and add it to the current thread's label. "
     "Returns the the newly created (integer) category."
    },
    {"get_id",  histar_self_get_id, METH_VARARGS,
     "Get the current thread's (integer) id."
    },
    {"get_label",  histar_self_get_label, METH_VARARGS,
     "Get the current thread's label.  Returns a histar.Label"
    },
    {"get_clearance",  histar_self_get_clearance, METH_VARARGS,
     "Get the current thread's clearance label.  Returns a histar.Label"
    },
    {"fs_create",  histar_fs_create, METH_VARARGS,
     "In the given dir create a file with the given label"
    },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC
init_histar(void)
{
    PyObject *m;

    histar_LabelType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&histar_LabelType) < 0)
        return;

    m = Py_InitModule3("_histar", HiStarMethods, "Internal HiStar Module");

    HiStarError = PyErr_NewException("histar.error", NULL, NULL);
    Py_INCREF(HiStarError);
    PyModule_AddObject(m, "error", HiStarError);

    Py_INCREF(&histar_LabelType);
    PyModule_AddObject(m, "Label", (PyObject *)&histar_LabelType);
}
