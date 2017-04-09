#ifndef COMMON_H_
#define COMMON_H_

#include <sys/types.h>
#include <sys/socket.h>

#define SA struct sockaddr_in
#define MAXBUF 8192

struct ConPair
{
  int descriptor;
  SA info;
};

struct ConPair
create_udp_socket(int port);

#endif
