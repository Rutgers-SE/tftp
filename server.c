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
#include <unistd.h>


#include "common.h"

#define DONE 0
#define PROGRESS 1
#define NEW 2

fd_set rfds;
struct timeval tv;
int retval;

struct Transfer
{
  int block_number;
  char prev_block_content[512];
  char filename[256];
  int port;
  int status;
};
#define T struct Transfer

struct Transfer transfers[12];
int btid = -1; // % 12
int tnum = 0;

int
transfers_get(int port)
{
  int i = 0;
  for (; i < tnum; i++)
    {
      if (transfers[i].port == port && (transfers[i].status == NEW || transfers[i].status == PROGRESS))
        return i;
    }
  return -1;
}


// precondition: port must be run through ntohs before using this function
// returns the index of the transfer
int
register_client_transfer(char filename[256], int port)
{
  btid = (btid+1)%12; // incrementing the port
  strcpy(transfers[btid].filename, filename);
  transfers[btid].port = port;
  transfers[btid].block_number=1;
  transfers[btid].status=NEW;
  int index = btid;
  tnum++; // bump the count
  return index;
}


int
transfers_block(int fd, int tid, SA *cad, socklen_t len)
{
  int bytes_read;
  char buf[512];
  T *tfs = transfers + tid;

  FILE* f = fopen(tfs->filename, "rb");
  int offset = 512 * (tfs->block_number-1);
  fseek(f, offset, SEEK_SET);

  bytes_read = fread(buf, 1, 512, f);

  if (bytes_read <= 0)
    {
      printf("bytes read %i\n", bytes_read);
      tfs->status = DONE;
      return -1;
    }

  memcpy(tfs->prev_block_content, buf, bytes_read);

  if (send_data_packet(fd,
                       tfs->block_number,
                       buf,
                       bytes_read,
                       cad,
                       len))
    {
      printf("Transfer Failed");
      return -1;
    }
  tfs->status = PROGRESS;
  tfs->block_number = tfs->block_number + 1;
  fclose(f);
  return 0;
}


int
handle_read_request(int fd, char* command, int n, SAI cad, size_t len)
{
  RRP rrp;
  parse_rrp(&rrp, command, n);

  printf("[Read Request filename=%s mode=%s port=%i tip=%s]\n",
         rrp.filename,
         rrp.mode,
         ntohs(cad.sin_port),
         inet_ntoa(cad.sin_addr));

  int tid = register_client_transfer(rrp.filename, ntohs(cad.sin_port)); /* NOTE: not sure if I need to do this. */

  transfers_block(fd, tid, (SA*)&cad, len);

  return tid;
}

void
tftp_handler(int fd, struct sockaddr_in *sinfo, socklen_t clilen)
{
  int n;

  // EVENT LOOP
  while (1)
    {
      // this is where multiplexing will come into play
      FD_ZERO(&rfds);
      FD_SET(fd, &rfds);
      tv.tv_sec = 1; // only wait one second
      tv.tv_usec = 0;

      sleep(1);
      int sel_status = select(8, &rfds, NULL, NULL, &tv);

      if (sel_status == -1)
        exit(1);
      else if (sel_status == 0)
        {
          printf(".");
          fflush(stdout);
          continue;
        }
      // Ending multiplexer code
      printf("\n");

      struct sockaddr_in cad;
      socklen_t len = sizeof(cad);
      char command[MAXBUF];
      n = recvfrom(fd, command, MAXBUF, 0, (SA*)&cad, &len);
      command[n] = '\0';
      printf("%i bytes received from port %i\n", n, ntohs(cad.sin_port));



      uint16_t op = parse_op(command);
      if (op == OP_RRQ)
        { // handling a read request
          handle_read_request(fd, command, n, cad, len);
        }

      else if (op == OP_ACK)
        {
          AKP akp;
          parse_ack(&akp, command);
          printf("[Ack block_number=%i]\n", akp.block_number);
          int tid = transfers_get(ntohs(cad.sin_port));
          transfers_block(fd, tid, (SA*)&cad, len);
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
          char* response;
          int response_length;
          response = pack_erp(&response_length, 1);
          sendto(fd, response, response_length, 0, (SA*)&cad, len);
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
  /* Map m; */
  /* map_init(&m); */

  /* int key = 0; */
  /* int value = 32; */
  /* map_add(&key, &value, &m); */
  /* int fetched_value = *((int*)map_get(&key, &m)); */
  /* printf("%i\n", fetched_value); */


  // setting up multiplexer
  bzero(&transfers, sizeof(transfers));

  int port = 10001;
  if (argc >= 2)
    port = atoi(argv[1]);

  struct ConPair cp = create_udp_socket(port);
  bind(cp.descriptor, (SA*)&cp.info, sizeof(cp.info));
  printf("Created UDP socket on %d.\n", port);

  tftp_handler(cp.descriptor, (struct sockaddr_in *)&cp.info, sizeof(cp.info));

  close(cp.descriptor);
}
