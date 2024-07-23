#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "raft.h"

#define NUM_NODES 5

typedef struct {
    raft_server_t *raft;
    raft_node_t *nodes[NUM_NODES];
    int node_id;
} server_t;

server_t servers[NUM_NODES];

static int __send_requestvote(raft_server_t* raft, void *user_data, raft_node_t* node, msg_requestvote_t* msg) {
    server_t *server = (server_t*)user_data;
    int target_node = raft_node_get_id(node) - 1;
    
    printf("Node %d sending requestvote to Node %d\n", server->node_id, target_node + 1);
    
    msg_requestvote_response_t response;
    raft_recv_requestvote(servers[target_node].raft, servers[target_node].nodes[server->node_id - 1], msg, &response);
    raft_recv_requestvote_response(raft, node, &response);
    
    return 0;
}

// Callback for sending appendentries messages
static int __send_appendentries(raft_server_t* raft, void *user_data, raft_node_t* node, msg_appendentries_t* msg) {
    server_t *server = (server_t*)user_data;
    int target_node = raft_node_get_id(node) - 1;
    
    printf("Node %d sending appendentries to Node %d\n", server->node_id, target_node + 1);
    
    msg_appendentries_response_t response;
    raft_recv_appendentries(servers[target_node].raft, servers[target_node].nodes[server->node_id - 1], msg, &response);
    raft_recv_appendentries_response(raft, node, &response);
    
    return 0;
}

// Callback for applying log entries to the state machine
static int __applylog(raft_server_t* raft, void *user_data, raft_entry_t *entry, raft_index_t entry_idx) {
    server_t *server = (server_t*)user_data;
    printf("Node %d applying log entry %ld\n", server->node_id, entry_idx);
    return 0;
}

// Callback for persisting vote data
static int __persist_vote(raft_server_t* raft, void *user_data, raft_node_id_t vote) {
    server_t *server = (server_t*)user_data;
    printf("Node %d persisting vote for node %d\n", server->node_id, vote);
    return 0;
}

// Callback for persisting term data
static int __persist_term(raft_server_t* raft, void *user_data, raft_term_t term, raft_node_id_t vote) {
    server_t *server = (server_t*)user_data;
    printf("Node %d persisting term %ld and vote for node %d\n", server->node_id, term, vote);
    return 0;
}

// Callback for logging debug messages
static void __log(raft_server_t* raft, raft_node_t* node, void *user_data, const char *buf) {
    server_t *server = (server_t*)user_data;
    printf("Node %d: %s\n", server->node_id, buf);
}

int main() {
    srand(time(NULL));
    
    raft_cbs_t raft_callbacks = {
        .send_requestvote = __send_requestvote,
        .send_appendentries = __send_appendentries,
        .applylog = __applylog,
        .persist_vote = __persist_vote,
        .persist_term = __persist_term,
        .log = __log
    };

    // Initialize raft servers
    for (int i = 0; i < NUM_NODES; i++) {
        servers[i].raft = raft_new();
        servers[i].node_id = i + 1;
        raft_set_callbacks(servers[i].raft, &raft_callbacks, &servers[i]);
    }

    // Add nodes
    for (int i = 0; i < NUM_NODES; i++) {
        for (int j = 0; j < NUM_NODES; j++) {
            servers[i].nodes[j] = raft_add_node(servers[i].raft, NULL, j + 1, i == j);
        }
    }

    // Run algorithm
    for (int iter = 0; iter < 20; iter++) {
        printf("\n--- Iteration %d ---\n", iter + 1);
        for (int i = 0; i < NUM_NODES; i++) {
            raft_periodic(servers[i].raft, 100 + (rand() % 200));
        }
        usleep(500000);
    }

    // Final States
    printf("\n--- Final States ---\n");
    for (int i = 0; i < NUM_NODES; i++) {
        printf("Node %d - State: %s, Term: %ld, Leader: %d\n",
               i + 1,
               raft_get_state(servers[i].raft) == RAFT_STATE_LEADER ? "Leader" :
               raft_get_state(servers[i].raft) == RAFT_STATE_CANDIDATE ? "Candidate" : "Follower",
               raft_get_current_term(servers[i].raft),
               raft_get_current_leader(servers[i].raft));
    }

    for (int i = 0; i < NUM_NODES; i++) {
        raft_free(servers[i].raft);
    }

    return 0;
}