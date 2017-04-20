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
#include <time.h>

#include "common.h"

#define DONE 0
#define PROGRESS 1
#define NEW 2
#define MAXTRAN 12


// start multiplexing globals
fd_set rfds;
struct timeval tv;
int retval;
// end multiplexing globals

struct Transfer
{
  int block_number;
  char prev_block_content[512];
  char filename[256];
  int port;
  int status;
  int mode;


  /* S */
  struct ConPair cp;

  SAI cinfo;
  int clen;
};
#define T struct Transfer

struct Transfer transfers[MAXTRAN];


void
reset_fds()
{
  int i = 0;
  for (; i < MAXTRAN; i++)
    {
      if ((transfers[i].status == NEW || transfers[i].status == PROGRESS))
        {
          FD_SET(transfers[i].cp.descriptor, &rfds);
        }
    }
}

T*
transfers_get(int port)
{
  int i = 0;
  for (; i < MAXTRAN; i++)
    {
      if (transfers[i].cinfo.sin_port == port && (transfers[i].status == NEW || transfers[i].status == PROGRESS))
        return (transfers+i);
    }
  return NULL;
}

T*
get_done_transfer()
{
  int i;
  for (i=0; i<MAXTRAN; i++)
    {
      if (transfers[i].status == DONE)
        return (transfers + i);
    }

  return NULL;
}


// precondition: port must be run through ntohs before using this function
// returns the index of the transfer
T*
register_client_transfer(char filename[256], int mode, SAI cinfo, size_t len)
{
  srand(time(NULL));
  int new_port = (rand()%65535)+10000;
  T* t = get_done_transfer();
  t->cp = create_udp_socket(new_port);
  strcpy(t->filename, filename);

  t->block_number=1;
  t->status=NEW;

  t->cinfo = cinfo;
  t->clen = len;

  return t;
}

T*
get_ready_transfer()
{
  int selval = select(16, &rfds, NULL, NULL, &tv);
  int i;

  if (selval == -1)
    perror("Noooo");
  else if (selval)
    for (i=0; i<MAXTRAN; i++)
      if (FD_ISSET(transfers[i].cp.descriptor, &rfds))
        return (transfers + i);

  return NULL;
}



/* transfer_block(int fd, int tid, SA *cad, socklen_t len) */
int
transfer_block(T* tfs)
{
  int bytes_read;
  char buf[512];
  /* T *tfs = transfers + tid; */

  // Load a check of the file
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
  fclose(f);

  if (send_data_packet(tfs->cp.descriptor,
                       tfs->block_number,
                       buf,
                       bytes_read,
                       (SA*)&(tfs->cinfo),
                       tfs->clen))
    {
      printf("Transfer Failed");
      return -1;
    }

  tfs->status = PROGRESS;

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

  if (access(rrp.filename, F_OK) == -1)
    {
      int elen;
      char* erp = pack_erp(&elen, 1);
      sendto(fd, erp, elen, 0, (SA*)&cad, len);
      return 1;
    }

  // ntohs(cad.sin_port)
  int mode = get_mode(rrp.mode);
  T* t = register_client_transfer(rrp.filename,
                                  mode,
                                  cad,
                                  len
                                  );

  if (t != NULL)
    {
      transfer_block(t);
      return 0;
    }

  return 1;
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
      reset_fds();
      tv.tv_sec = 1; // only wait one second
      tv.tv_usec = 0;

      // sleep(1);
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


      T* t = get_ready_transfer();
      if (t == NULL)
        {
          n = recvfrom(fd, command, MAXBUF, 0, (SA*)&cad, &len);
          command[n] = '\0';
          printf("%i bytes received from port %i\n", n, ntohs(cad.sin_port));
        }
      else
        {
          n = recvfrom(t->cp.descriptor, command, MAXBUF, 0, (SA*)&cad, &len);
          command[n] = '\0';
          printf("%i bytes received from port %i\n", n, ntohs(cad.sin_port));
        }






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
          if (akp.block_number == t->block_number)
            t->block_number++;
          if (t != NULL)
            transfer_block(t);
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


  // setting up multiplexer
  bzero(&transfers, sizeof(transfers));
  int i;
  for (i=0; i<MAXTRAN; i++)
    {
      transfers[i].status = DONE;
    }

  int port = 10001;
  if (argc >= 2)
    port = atoi(argv[1]);

  struct ConPair cp = create_udp_socket(port);
  bind(cp.descriptor, (SA*)&cp.info, sizeof(cp.info));
  printf("Created UDP socket on %d.\n", port);

  tftp_handler(cp.descriptor, (struct sockaddr_in *)&cp.info, sizeof(cp.info));

  close(cp.descriptor);
}
