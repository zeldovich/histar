#ifndef FILECLIENT_HH_
#define FILECLIENT_HH_

int fileclient_addr(char *host, int port, struct sockaddr_in *addr);
int fileclient_socket(void);
int fileclient_connect(int s, struct sockaddr_in *addr);

#endif /*FILECLIENT_HH_*/
