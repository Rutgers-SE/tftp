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
      struct sockaddr_in cad;
      socklen_t len = sizeof(cad);

      char command[MAXBUF];
      char* response;
      int response_length;
      printf("Waiting for client\n");
      n = recvfrom(fd, command, MAXBUF, 0, &cad, &len);
      command[n] = '\0';

      printf("%i bytes received\n", n);

      uint16_t op = parse_op(command);
      if (op == OP_RRQ)
        { // handling a read request
          RRP rrp;
          parse_rrp(&rrp, command, n);

          printf("Read Request.\n\tfilename=%s\n\tmode=%s\n\tport=%i\n\tip=%s\n",
                 rrp.filename,
                 rrp.mode,
                 ntohs(cad.sin_port),
                 parse_ip(&cad));
          response = pack_erp(&response_length, 1);
        }
      else if (op == OP_WRQ)
        { // handling a write request
          WRP wrp;
          parse_wrp(&wrp, command, n);
          printf("Write Request.\n\tfilename=%s\n\tmode=%s\n\tport=%i\n\tip=%s\n",
                 wrp.filename,
                 wrp.mode,
                 ntohs(cad.sin_port),
                 parse_ip(&cad));
          response = pack_erp(&response_length, 1);
        }
      else
        {
          printf("Unsupported request\n");
        }

      sendto(fd, response, response_length, 0, &cad, len);
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
