#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "raft.h"
#include "raft_types.h"
#include "raft_log.h"
#include "raft_private.h"


#define MAX_NODES 2

typedef struct {
    raft_server_t* raft;
    pthread_mutex_t lock;
    int id;
} node_t;

node_t nodes[MAX_NODES];

typedef struct {
    int from;
    int to;
    int type; // 1 for RequestVote, 2 for AppendEntries
    union {
        msg_requestvote_t rv;
        msg_appendentries_t ae;
    } msg;
} message_t;

message_t msg_queue[100];
int msg_count = 0;

void send_message(int from, int to, int type, void* msg) {
    pthread_mutex_lock(&nodes[to].lock);
    msg_queue[msg_count].from = from;
    msg_queue[msg_count].to = to;
    msg_queue[msg_count].type = type;
    if (type == 1) {
        memcpy(&msg_queue[msg_count].msg.rv, msg, sizeof(msg_requestvote_t));
    } else if (type == 2) {
        memcpy(&msg_queue[msg_count].msg.ae, msg, sizeof(msg_appendentries_t));
    }
    msg_count++;
    pthread_mutex_unlock(&nodes[to].lock);
}

void process_messages(node_t* node) {
    pthread_mutex_lock(&node->lock);
    for (int i = 0; i < msg_count; i++) {
        if (msg_queue[i].to == node->id) {
            raft_node_t* sender_node = raft_get_node(node->raft, msg_queue[i].from);
            if (msg_queue[i].type == 1) {
                msg_requestvote_response_t r;
                raft_recv_requestvote(node->raft, sender_node, &msg_queue[i].msg.rv, &r);
            } else if (msg_queue[i].type == 2) {
                msg_appendentries_response_t r;
                raft_recv_appendentries(node->raft, sender_node, &msg_queue[i].msg.ae, &r);
            }
        }
    }
    pthread_mutex_unlock(&node->lock);
}

void* node_thread(void* arg) {
    node_t* node = (node_t*)arg;
    for (int i = 0; i < 10; i++) {
        raft_periodic(node->raft, 1000); // Simulate periodic callback every second
        process_messages(node);
        printf("Node %d periodic callback #%d\n", node->id, i);
        sleep(1);
    }
    return NULL;
}

static int my_send_requestvote(raft_server_t* raft, void *udata, raft_node_t *node, msg_requestvote_t* m) {
    (void)raft; (void)udata;
    int node_id = raft_node_get_id(node);
    send_message(((node_t*)udata)->id, node_id, 1, m);
    printf("Node %d sending RequestVote to node %d\n", ((node_t*)udata)->id, node_id);
    return 0;
}

static int my_send_appendentries(raft_server_t* raft, void *udata, raft_node_t *node, msg_appendentries_t* m) {
    (void)raft; (void)udata;
    int node_id = raft_node_get_id(node);
    send_message(((node_t*)udata)->id, node_id, 2, m);
    printf("Node %d sending AppendEntries to node %d\n", ((node_t*)udata)->id, node_id);
    return 0;
}

static int my_applylog(raft_server_t* raft, void *udata, raft_entry_t *ety, raft_index_t ety_idx) {
    (void)raft; (void)udata; (void)ety_idx;
    printf("Applying log entry with id %d\n", ety->id);
    return 0;
}

static int my_persist_vote(raft_server_t* raft, void *udata, raft_node_id_t vote) {
    (void)raft; (void)udata;
    printf("Persisting vote: %d\n", vote);
    return 0;
}

static int my_persist_term(raft_server_t* raft, void *udata, raft_term_t term, raft_node_id_t vote) {
    (void)raft; (void)udata;
    printf("Persisting term: %ld and vote: %d\n", term, vote);
    return 0;
}

static int my_log_offer(raft_server_t* raft, void *udata, raft_entry_t *ety, raft_index_t ety_idx) {
    (void)raft; (void)udata;
    printf("Offering log entry with id %d at index %ld\n", ety->id, ety_idx);
    return 0;
}

static int my_log_poll(raft_server_t* raft, void *udata, raft_entry_t *entry, raft_index_t ety_idx) {
    (void)raft; (void)udata; (void)ety_idx;
    printf("Polling log entry with id %d\n", entry->id);
    return 0;
}

static int my_log_pop(raft_server_t* raft, void *udata, raft_entry_t *entry, raft_index_t ety_idx) {
    (void)raft; (void)udata; (void)ety_idx;
    printf("Popping log entry with id %d\n", entry->id);
    return 0;
}

static void my_log(raft_server_t* raft, raft_node_t* node, void *udata, const char *buf) {
    (void)raft; (void)node; (void)udata;
    printf("Raft log: %s\n", buf);
}

static int my_node_has_sufficient_logs(raft_server_t* raft, void *udata, raft_node_t* node) {
    (void)raft; (void)udata;
    printf("Node %d has sufficient logs\n", raft_node_get_id(node));
    return 0;
}

raft_cbs_t my_callbacks = {
    .send_requestvote = my_send_requestvote,
    .send_appendentries = my_send_appendentries,
    .applylog = my_applylog,
    .persist_vote = my_persist_vote,
    .persist_term = my_persist_term,
    .log_offer = my_log_offer,
    .log_poll = my_log_poll,
    .log_pop = my_log_pop,
    .log = my_log,
    .node_has_sufficient_logs = my_node_has_sufficient_logs,
};

int main() {
    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].raft = raft_new();
        nodes[i].id = i;
        pthread_mutex_init(&nodes[i].lock, NULL);
        raft_set_callbacks(nodes[i].raft, &my_callbacks, &nodes[i]);
        raft_add_node(nodes[i].raft, &nodes[i], i, i == 0);
    }

    pthread_t threads[MAX_NODES];
    for (int i = 0; i < MAX_NODES; i++) {
        pthread_create(&threads[i], NULL, node_thread, &nodes[i]);
    }

    for (int i = 0; i < MAX_NODES; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < MAX_NODES; i++) {
        raft_free(nodes[i].raft);
        pthread_mutex_destroy(&nodes[i].lock);
    }

    return 0;
}
