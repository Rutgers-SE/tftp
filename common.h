#ifndef COMMON_H_
#define COMMON_H_

#include <sys/types.h>
#include <sys/socket.h>

#define SA struct sockaddr_in
#define MAXBUF 8192
#define MAX_STRING_LEN 8192
#define OP_RRQ 1
#define OP_WRQ 2
#define OP_DAT 3
#define OP_ACK 4
#define OP_ERR 5

#pragma pack(1)


struct ConPair
{
  int descriptor;
  SA info;
};

#define AKP struct AckPack
struct AckPack
{
  uint16_t opcode;
  uint16_t block_number;
};

#define ERP struct ErrPack
struct ErrPack
{
  uint16_t opcode;
  uint16_t errcode;
  char *err_msg;
};

#define RRP struct ReadReqPack
struct ReadReqPack
{
  uint16_t opcode;
  char *filename;
  char *mode;
};

#define WRP struct WriteReqPack
struct WriteReqPack
{
  uint16_t opcode;
  char *filename;
  char *mode;
};

#define DAP struct DataPack
struct DataPack
{
  uint16_t opcode;
  uint16_t block_number;
  uint8_t data;
};

struct ConPair
create_udp_socket(int port);
char *create_write_request_packet(char*, char*);
uint16_t parse_op(char* buf);
void pack_ack(AKP* buf, uint16_t block_number);
void parse_ack(AKP* akp, char* buf);

char* pack_erp(int* len, int err_code);
void parse_erp(ERP* erp, char* buf, int buffer_size);

char* pack_rrp(int* len, char* filename, char*mode);
void parse_rrp(RRP* rrp, char* buf, int buffer_size);

char* pack_wrp(int* len, char* filename, char*mode);
void parse_wrp(WRP* wrp, char* buf, int buffer_size);

#endif
