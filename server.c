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
      char command[MAXBUF];
      len = clilen;
      printf("Waiting for client\n");
      n = recvfrom(fd, command, MAXBUF, 0, sinfo, &len);
      command[n] = '\0';

      printf("%i bytes received\n", n);

      uint16_t op = parse_op(command);
      if (op == OP_RRQ)
        {
          ERP erp; // for now we are returning an error
          RRP rrp;
          printf("Someone wants to read a file\n");
          parse_rrp(&rrp, command, n);
          /* pack_erp(&erp, "File Not Found") */
        }
      else if (op == OP_WRQ)
        {
          printf("Someone wants to write a file\n");
        }
      else
        {
          printf("Unsupported request\n");
        }

      sendto(fd, msg, n, 0, sinfo, len);
    }
}

int
main(int argc, char **argv)
{
  struct ConPair cp = create_udp_socket(10001);
  bind(cp.descriptor, (SA *)&cp.info, sizeof(cp.info));

  dg_echo(cp.descriptor, (struct sockaddr_in *)&cp.info, sizeof(cp.info));

  close(cp.descriptor);
}
