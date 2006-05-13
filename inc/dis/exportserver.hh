#ifndef EXPORTSERVER_HH_
#define EXPORTSERVER_HH_

class export_server
{
public:
    export_server(int port) : port_(port), running_(false) {}

    bool running(void) const { return running_;}
    void running_is(bool running);

private:
    uint16_t port_;
    bool     running_;    
};

#endif /*EXPORTSERVER_HH_*/
