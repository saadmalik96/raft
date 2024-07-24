#ifndef RAFT_NET_H
#define RAFT_NET_H

#include <arpa/inet.h>
#include "raft.h"

#define MAX_BUFFER_SIZE 1024
#define BASE_PORT 5000

typedef enum {
    MSG_REQUESTVOTE,
    MSG_REQUESTVOTE_RESPONSE,
    MSG_APPENDENTRIES,
    MSG_APPENDENTRIES_RESPONSE
} message_type_t;

typedef struct {
    message_type_t type;
    int sender_id;
    union {
        msg_requestvote_t rv;
        msg_requestvote_response_t rvr;
        msg_appendentries_t ae;
        msg_appendentries_response_t aer;
    } msg;
} network_message_t;

void serialize_message(network_message_t *msg, char *buffer);
void deserialize_message(char *buffer, network_message_t *msg);

#endif