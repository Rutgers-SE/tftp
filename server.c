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
int ts = 0;
int te = MAXTRAN-1;
void rotate()
{
  ts = (ts+1)%MAXTRAN;
  te = (te+1)%MAXTRAN;
}



void
reset_fds()
{
  int i = 0;
  for (; i < MAXTRAN; i++)
    if ((transfers[i].status == NEW || transfers[i].status == PROGRESS))
      FD_SET(transfers[i].cp.descriptor, &rfds);
}

T*
transfers_get(int port)
{
  rotate();
  int i = ts;
  while (((i+1)%MAXTRAN) != te)
    {
      if (transfers[i].cinfo.sin_port == port && (transfers[i].status == NEW || transfers[i].status == PROGRESS))
        return (transfers+i);
      i=(i+1)%MAXTRAN;
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
  // generate a new random port number
  srand(time(NULL));
  int new_port = (rand()%65535)+10000;

  T* t = get_done_transfer();   /* get a transfer struct that is marked as done. */

  t->cp = create_udp_socket(new_port); /* create a new socket for the transfer */
  bind(t->cp.descriptor, (SA*)&(t->cp.info), sizeof(t->cp.info));

  // set potential useful information
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
  rotate();                     /* used to make sure 'fair' use of transfers */
  int selval = select(128, &rfds, NULL, NULL, &tv);
  //----------------->^^^<-- Don't ask me why
  int i = ts;

  if (selval == -1)
    perror("Select threw an error\n");
  else if (selval)
    while (((i+1)%MAXTRAN)!=te)
      {
        if (FD_ISSET(transfers[i].cp.descriptor, &rfds) &&
            transfers[i].status != DONE)
          return (transfers+i);
        i=(i+1)%MAXTRAN;
      }

  return NULL;
}



/* transfer_block(int fd, int tid, SA *cad, socklen_t len) */
int
transfer_block(T* tfs)
{
  int bytes_read;
  char buf[512];

  // Load a check of the file
  FILE* f = fopen(tfs->filename, "rb");
  int offset = 512*(tfs->block_number-1); /* goto the proper location in the file */
  fseek(f, offset, SEEK_SET);
  bytes_read = fread(buf, 1, 512, f);
  if (bytes_read <= 0)
    {
      printf("(%i\t%i)\t[DONE]\n", tfs->cp.descriptor, tfs->cinfo.sin_port);
      tfs->status = DONE;       /* mark transfer as done */
      close(tfs->cp.descriptor);
      FD_CLR(tfs->cp.descriptor, &rfds);
      bzero(tfs, sizeof(*tfs));
      fclose(f);
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

  printf("RRQ()\t<--\t[FD(%i)\tPORT(%i)\t%s\t%s]\n",
         fd,
         ntohs(cad.sin_port),
         rrp.filename,
         rrp.mode);

  if (access(rrp.filename, F_OK) == -1)
    {
      // The file does not exist, send an error packet
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
                                  len);

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
      FD_ZERO(&rfds);           /* reset the read set */
      FD_SET(fd, &rfds);        /* add the port 69 file descriptor */
      reset_fds();              /* reset current running transfer file descriptors */
      tv.tv_sec = 1;            /* reset the timer for transfers */
      tv.tv_usec = 0;

      struct sockaddr_in cad;
      socklen_t len = sizeof(cad);
      char command[MAXBUF];


      T* t = get_ready_transfer();
      if (t == NULL)
        {
          if (FD_ISSET(fd, &rfds))
            {
              n = recvfrom(fd, command, MAXBUF, 0, (SA*)&cad, &len);
              command[n] = '\0';
            }
          else
            continue;
        }
      else
        {
          n = recvfrom(t->cp.descriptor, command, MAXBUF, 0, (SA*)&cad, &len);
          command[n] = '\0';
        }

      uint16_t op = parse_op(command);
      if (op == OP_RRQ)
        { // handling a read request
          handle_read_request(fd, command, n, cad, len);
        }

      else if (op == OP_ACK)
        {
          if (t == NULL) continue;
          AKP akp;
          parse_ack(&akp, command);

          /* printf("ACK(%i)\t<--\t[]\n", */
          /*        akp.block_number); */
          printf("ACK(%i)\t<--\t[FD(%i)\tPORT(%i)]\n",
                 akp.block_number,
                 fd,
                 ntohs(cad.sin_port));

          // increment the block number if recv.
          if (akp.block_number == t->block_number)
            t->block_number++;
          else if (akp.block_number < t->block_number)
            t->block_number--;

          // transfer a block
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
