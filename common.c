#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "common.h"
#include <stdint.h>
#include <strings.h>
#include <string.h>


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

char
*create_write_packet(char* filename, char* mode)
{
  char *buf = malloc(4 + strlen(filename) + strlen(mode) + 2);
}


// composes ACK packet and puts it into buffer
void
pack_ack(AKP* buf, uint16_t block_number)
{
  buf->opcode = htons(OP_ACK);
  buf->block_number = htons(block_number);
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

void
htonc(char* buf, char* out, int size)
{
  for (int i=0; i<size;i+=2)
    {
      uint16_t bb = buf[i];
      bb  = (bb << 8) | buf[i+1];

      uint16_t nb = htons(bb);
      out[i] = nb >> 8;
      out[i+1] = (nb << 8) >> 8;

      printf("%i %c %c\n", nb, out[i], out[i+1]);
    }
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

  buf[0] = OP_RRQ << 8;
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
  for (int i = 2; i < size; i++)
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
  for (int i = nxt; i < size; i++)
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
  char error_message[MAXBUF];
  erp->opcode = parse_op(buf);
  uint16_t ec;
  ec = buf[2];
  ec = (ec<< 8) | buf[3];
  erp->errcode = ec;
  erp->err_msg = get_error_message((int)ec);
}

uint16_t
parse_op(char* buf)
{
  uint16_t op;
  op = buf[0];
  op = (op << 8) | buf[1];
  return op;
}

char*
parse_ip(struct sockaddr_in *client)
{
  return inet_ntoa(client->sin_addr);
}

