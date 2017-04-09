#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <strings.h>
#include "common.h"

void
dg_echo(int fd, struct sockaddr_in *sinfo, socklen_t clilen)
{
  int n;
  socklen_t len;
  char msg[MAXBUF];

  while (1)
    {
      len = clilen;
      printf("Waiting for client\n");
      n = recvfrom(fd, msg, MAXBUF, 0, sinfo, &len);
      printf("Awesome\n");
      sendto(fd, msg, n, 0, sinfo, len);
    }
}

int
main(int argc, char **argv)
{
  struct ConPair cp = create_udp_socket(10001);
  bind(cp.descriptor, (struct sockaddr_in *)&cp.info, sizeof(cp.info));

  dg_echo(cp.descriptor, (struct sockaddr_in *)&cp.info, sizeof(cp.info));

  close(cp.descriptor);
}
