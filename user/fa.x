struct fadd_arg {
    string var<>;
    int inc;
};

union fadd_res switch (int error) {
 case 0:
     int sum;
 default:
     void;
};

program FADD_PROG {
    version FADD_VERS {
	void FADDPROC_NULL (void) = 0;
	fadd_res FADDPROC_FADD (fadd_arg) = 1;
    } = 1;
} = 300001;
