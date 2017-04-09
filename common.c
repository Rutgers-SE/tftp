#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "common.h"

struct ConPair
create_udp_socket(int port)
{
  struct ConPair cp;
  if ((cp.descriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      printf("Error creating the socket.");
      exit(1);
    }

  bzero(&cp.info, sizeof(cp.info));
  cp.info.sin_family = AF_INET;
  cp.info.sin_addr.s_addr = htonl(INADDR_ANY);
  cp.info.sin_port = htons(port);

  return cp;
}
