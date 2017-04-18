#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

int
main(int argc, char **argv)
{
  struct ConPair cp = create_udp_socket(10001);

  int len, lenw;
  char *ruf = pack_rrp(&len, "protocol", "ascii");
  char *wuf = pack_wrp(&lenw, "another", "test");

  char response[MAXBUF];


  printf("Sending %i bytes\n", len);

  sendto(cp.descriptor, ruf, len, 0, (SAI*)&cp.info, sizeof(cp.info));

  int nn;
  int n = recvfrom(cp.descriptor, response, MAXBUF, 0,
                   (SAI *)&cp.info,
                   &nn
                   // sizeof(cp.info)
                   );
  response[nn] = '\0';
  printf("Received %i bytes\n", nn);

  int op = parse_op(response);
  if (op == 5)
    {
      ERP erp;
      parse_erp(&erp, response, n);
      printf("Error from server.\n\tcode=%i\n\tmessage=%s\n\n",
             erp.errcode,
             erp.err_msg);
    }


  printf("Sending %i bytes\n", lenw);
  sendto(cp.descriptor, wuf, lenw, 0, (SAI*)&cp.info, sizeof(cp.info));
  nn=0;
  n = recvfrom(cp.descriptor, response, MAXBUF, 0,
                   (SAI*)&cp.info,
                   &nn
                   // sizeof(cp.info)
                   );
  response[nn] = '\0';
  printf("Received %i bytes\n", nn);
  op = parse_op(response);
  if (op == 5)
    {
      ERP erp;
      parse_erp(&erp, response, n);
      printf("Error from server.\n\tcode=%i\n\tmessage=%s\n\n",
             erp.errcode,
             erp.err_msg);
    }



  printf("Closing Connection\n");
  close(cp.descriptor);


  return 0;
}
