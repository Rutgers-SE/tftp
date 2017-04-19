#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "common.h"
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <sys/select.h>


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

// returns block number from ACK
void
parse_ack(AKP* akp, char* buf)
{
  akp->opcode = buf[0];
  akp->opcode = (akp->opcode << 8) | buf[1];

  akp->block_number = buf[2];
  akp->block_number = (akp->block_number << 8) | buf[3];
}

char*
pack_rrp(int* len, char* filename, char* mode)
{
  *len = 2 +                 // opcode
    strlen(filename) +          // filename
    1 +                         // null byte
    strlen(mode) +              // mode
    1;                          // null byte
  char* buf = malloc(*len);

  buf[0] = OP_RRQ >> 8;
  buf[1] = OP_RRQ;

  strcpy(buf+2, filename);
  buf[2 + strlen(filename)] = 0;

  strcpy(buf+3+strlen(filename), mode);
  buf[2+strlen(filename)+1+strlen(mode)] = 0;
  return buf;
}

void
parse_rrp(RRP* rrp, char* buf, int size)
{
  char filename[MAXBUF];
  char mode[MAXBUF];
  int nxt;

  // THIS LOOP GETS THE FILENAME
  int mi = 0;
  int i;
  for (i = 2; i < size; i++)
    if (buf[i] != '\0')
      {
        /* printf("Character[%i] %c %i\n", i, buf[i], buf[i]); */
        filename[mi] = buf[i];
        mi++;
      }
    else
      {
        /* printf("Reached NULL character at %i\n", i); */
        filename[mi] = '\0';
        nxt = i+1;
        break;
      }


  // THIS LOOP GETS THE MODE
  mi = 0;
  for (i = nxt; i < size; i++)
    if (buf[i] != '\0')
      {
        /* printf("Character[%i] %c %i\n", i, buf[i], buf[i]); */
        mode[mi] = buf[i];
        mi++;
      }
    else
      {
        /* printf("Reached NULL character at %i\n", i); */
        mode[mi] = '\0';
        break;
      }

  rrp->opcode = parse_op(buf);
  rrp->filename = filename;
  rrp->mode = mode;

}



char*
pack_wrp(int* len, char* filename, char* mode)
{
  *len = 2 +                 // opcode
    strlen(filename) +          // filename
    1 +                         // null byte
    strlen(mode) +              // mode
    1;                          // null byte
  char* buf = malloc(*len);

  buf[0] = OP_WRQ >> 8;
  buf[1] = OP_WRQ;

  strcpy(buf+2, filename);
  buf[2 + strlen(filename)] = 0;

  strcpy(buf+3+strlen(filename), mode);
  buf[2+strlen(filename)+1+strlen(mode)] = 0;
  return buf;
}

void
parse_wrp(WRP* wrp, char* buf, int size)
{
  char filename[MAXBUF];
  char mode[MAXBUF];
  int nxt=0;

  // THIS LOOP GETS THE FILENAME
  int mi = 0;
  int i;
  for (i = 2; i < size; i++)
    if (buf[i] != '\0')
      {
        /* printf("Character[%i] %c %i\n", i, buf[i], buf[i]); */
        filename[mi] = buf[i];
        mi++;
      }
    else
      {
        /* printf("Reached NULL character at %i\n", i); */
        filename[mi] = '\0';
        nxt = i+1;
        break;
      }


  // THIS LOOP GETS THE MODE
  mi = 0;
  for (i = nxt; i < size; i++)
    if (buf[i] != '\0')
      {
        /* printf("Character[%i] %c %i\n", i, buf[i], buf[i]); */
        mode[mi] = buf[i];
        mi++;
      }
    else
      {
        /* printf("Reached NULL character at %i\n", i); */
        mode[mi] = '\0';
        break;
      }

  wrp->opcode = parse_op(buf);
  wrp->filename = filename;
  wrp->mode = mode;
}


char*
pack_dat(int* len, int block_number, char* data, int data_len)
{
  *len = 2 // op
    + 2 // block number
    + data_len;
  char *buf = malloc(*len);
  buf[0]=0;
  buf[1]=OP_DAT;
  buf[2]=block_number>>8;
  buf[3]=block_number;

  strncpy(buf+4, data, data_len); // could use mem copy for binary files

  return buf;
}


char*
get_error_message(int err_code)
{
  switch (err_code)
    {
    case 0:
      return "Not defined.";
    case 1:
      return "File not found.";
    case 2:
      return "Access violation.";
    case 3:
      return "Disk full or allocation exceeded.";
    case 4:
      return "Illegal TFTP operation.";
    default:
      exit(1);
    }
}

char*
pack_erp(int* len, int err_code)
{
  char* error_message = get_error_message(err_code);
  *len = 2 + // op code
    2 + // error code
    strlen(error_message) +  // string len
    1; // null byte

  char* buf = malloc(*len);
  buf[0] = 0;
  buf[1] = 5;
  buf[2] = 0;
  buf[3] = err_code;
  strcpy(buf+4, error_message);
  return buf;
}

void
parse_erp(ERP* erp, char* buf, int buffer_size)
{
  erp->opcode = parse_op(buf);
  uint16_t ec;
  ec = buf[2];
  ec = (ec<<8)|buf[3];
  erp->errcode = ec;
  erp->err_msg = get_error_message((int)ec);
}

uint16_t
parse_op(char* buf)
{
  uint16_t op;
  op = buf[0];
  op = (op<<8)|buf[1];
  return op;
}

int
send_data_packet(int fd, int block_number, char* data, size_t size, SA* cad, socklen_t cadlen)
{
  // send packet to file descriptor
  int packet_size;
  char *dat = pack_dat(&packet_size, block_number, data, size);

  if (sendto(fd, dat, packet_size, 0, cad, cadlen) < 0)
    {
      return 1;
    }

  return 0;
}
