#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>


#include "common.h"

fd_set rfds;
struct timeval tv;
int retval;


int
handle_read_request(int fd, char* command, int n, struct sockaddr_in cad, size_t len)
{
  RRP rrp;
  char buf[512];
  int block_number = 1;
  int bytes_read;

  parse_rrp(&rrp, command, n);

  printf("Read Request.\n\tfilename=%s\n\tmode=%s\n\tport=%i\n\tip=%s\n",
         rrp.filename,
         rrp.mode,
         ntohs(cad.sin_port),
         inet_ntoa(cad.sin_addr));

  FILE *tf = fopen(rrp.filename, "rb");
  fseek(tf, 0, SEEK_END);
  int file_size = ftell(tf);
  fseek(tf, 0, SEEK_SET);

  /* int total_sends = file_size / 512; */
  int total_sends = ((file_size + (512/2)) / 512)+1;

  if (tf == NULL)
    {
      printf("File Does not Exist");
      return 1;
    }

  while (1)
    {
      if ((bytes_read = fread(buf, 1, 512, tf)) == 0)
        {
          fclose(tf);
          break;
        }
      if (send_data_packet(fd,
                           block_number,
                           buf,
                           bytes_read,
                           (SAI*)&cad,
                           len))
        {
          printf("Transfer Failed");
          return 1;
        }
      int percent_complete = ((float)block_number/(float)total_sends) * (float)100;
      printf("%c[2K", 27);
      printf("(%i\%)\n", percent_complete);
      block_number++;
    }
  return 0;
}


void
dg_echo(int fd, struct sockaddr_in *sinfo, socklen_t clilen)
{
  int n;
  socklen_t len;

  while (1)
    {
      struct sockaddr_in cad;
      socklen_t len = sizeof(cad);

      char command[MAXBUF];
      char* response;
      int response_length;
      printf("Waiting for client\n");
      n = recvfrom(fd, command, MAXBUF, 0, (SAI*)&cad, &len);
      command[n] = '\0';

      printf("%i bytes received\n", n);

      uint16_t op = parse_op(command);
      if (op == OP_RRQ)
        { // handling a read request
          handle_read_request(fd, command, n, cad, len);
        }
      else if (op == OP_WRQ)
        { // handling a write request
          WRP wrp;
          parse_wrp(&wrp, command, n);
          printf("Write Request.\n\tfilename=%s\n\tmode=%s\n\tport=%i\n\tip=%s\n",
                 wrp.filename,
                 wrp.mode,
                 ntohs(cad.sin_port),
                 inet_ntoa(cad.sin_addr));
          response = pack_erp(&response_length, 1);
          sendto(fd, response, response_length, 0, (SAI*)&cad, len);
        }
      else
        {
          printf("Unsupported request\n");
        }
    }
}

int
main(int argc, char **argv)
{
  // setting up multiplexer
  FD_ZERO(&rfds);

  tv.tv_sec = 5;
  tv.tv_usec = 0;



  int port = 10001;
  if (argc >= 2)
    {
      port = atoi(argv[1]);
    }
  struct ConPair cp = create_udp_socket(port);
  bind(cp.descriptor, (SAI *)&cp.info, sizeof(cp.info));
  printf("Created UDP socket on %d.\n", port);

  dg_echo(cp.descriptor, (struct sockaddr_in *)&cp.info, sizeof(cp.info));

  close(cp.descriptor);
}
