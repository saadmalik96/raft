#include <string.h>
#include <stdio.h>
#include "raft_net.h"

void serialize_message(network_message_t *msg, char *buffer) {
    sprintf(buffer, "%d %d ", msg->type, msg->sender_id);
    
    switch (msg->type) {
        case MSG_REQUESTVOTE:
            sprintf(buffer + strlen(buffer), "%ld %d %ld %ld",
                    msg->msg.rv.term, msg->msg.rv.candidate_id,
                    msg->msg.rv.last_log_idx, msg->msg.rv.last_log_term);
            break;
        case MSG_REQUESTVOTE_RESPONSE:
            sprintf(buffer + strlen(buffer), "%ld %d",
                    msg->msg.rvr.term, msg->msg.rvr.vote_granted);
            break;
        case MSG_APPENDENTRIES:
            sprintf(buffer + strlen(buffer), "%ld %ld %ld %ld %d",
                    msg->msg.ae.term, msg->msg.ae.prev_log_idx,
                    msg->msg.ae.prev_log_term, msg->msg.ae.leader_commit,
                    msg->msg.ae.n_entries);
            // Note: This doesn't serialize the actual entries. You'd need to implement that.
            break;
        case MSG_APPENDENTRIES_RESPONSE:
            sprintf(buffer + strlen(buffer), "%ld %d %ld %ld",
                    msg->msg.aer.term, msg->msg.aer.success,
                    msg->msg.aer.current_idx, msg->msg.aer.first_idx);
            break;
    }
}

void deserialize_message(char *buffer, network_message_t *msg) {
    sscanf(buffer, "%d %d", (int*)&msg->type, &msg->sender_id);
    
    char *ptr = strchr(buffer, ' ');
    ptr = strchr(ptr + 1, ' ');
    
    switch (msg->type) {
        case MSG_REQUESTVOTE:
            sscanf(ptr, "%ld %d %ld %ld",
                   &msg->msg.rv.term, &msg->msg.rv.candidate_id,
                   &msg->msg.rv.last_log_idx, &msg->msg.rv.last_log_term);
            break;
        case MSG_REQUESTVOTE_RESPONSE:
            sscanf(ptr, "%ld %d",
                   &msg->msg.rvr.term, &msg->msg.rvr.vote_granted);
            break;
        case MSG_APPENDENTRIES:
            sscanf(ptr, "%ld %ld %ld %ld %d",
                   &msg->msg.ae.term, &msg->msg.ae.prev_log_idx,
                   &msg->msg.ae.prev_log_term, &msg->msg.ae.leader_commit,
                   &msg->msg.ae.n_entries);
            // Note: This doesn't deserialize the actual entries. You'd need to implement that.
            break;
        case MSG_APPENDENTRIES_RESPONSE:
            sscanf(ptr, "%ld %d %ld %ld",
                   &msg->msg.aer.term, &msg->msg.aer.success,
                   &msg->msg.aer.current_idx, &msg->msg.aer.first_idx);
            break;
    }
}