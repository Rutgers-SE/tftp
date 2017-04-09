#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include "common.h"

int
main(int argc, char **argv)
{
  struct ConPair cp = create_udp_socket(10001);

  char message[MAXBUF] = "This is a test";
  char response[MAXBUF];
  AKP buf;
  char *ruf;

  pack_ack(&buf, 9);
  int len = pack_rrp(ruf, "awesome", "id");

  printf("Sending %i bytes\n", len);

  sendto(cp.descriptor, ruf, len, 0, (SA*)&cp.info, sizeof(cp.info));

  int n = recvfrom(cp.descriptor, response, MAXBUF, 0,
                   (SA *)&cp.info, sizeof(cp.info));
  response[n] = '\0';

  printf("This if from the response: %s\n", response);
  close(cp.descriptor);


  return 0;
}
