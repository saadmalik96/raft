#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "raft.h"
#include "raft_net.h"

#define NUM_NODES 3
extern const char* NODE_IPS[NUM_NODES];

const char* NODE_IPS[NUM_NODES] = {
    "127.0.0.1",
    "127.0.0.1",
    "127.0.0.1"
    // "127.0.0.1",
    // "127.0.0.1"
};

typedef struct {
    raft_server_t *raft;
    raft_node_t *nodes[NUM_NODES];
    int node_id;
    int server_socket;
} server_t;

server_t server;

static int send_message(raft_server_t* raft, raft_node_t* node, message_type_t type, void* msg) {
    int target_node = raft_node_get_id(node) - 1;
    struct sockaddr_in addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(NODE_IPS[target_node]);
    addr.sin_port = htons(BASE_PORT + target_node);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Connect failed");
        close(sock);
        return -1;
    }
    
    network_message_t net_msg;
    net_msg.type = type;
    net_msg.sender_id = server.node_id;
    memcpy(&net_msg.msg, msg, sizeof(net_msg.msg));
    
    char buffer[MAX_BUFFER_SIZE];
    serialize_message(&net_msg, buffer);
    
    send(sock, buffer, strlen(buffer), 0);
    close(sock);
    
    return 0;
}

static int __send_requestvote(raft_server_t* raft, void *user_data, raft_node_t* node, msg_requestvote_t* msg) {
    return send_message(raft, node, MSG_REQUESTVOTE, msg);
}

static int __send_appendentries(raft_server_t* raft, void *user_data, raft_node_t* node, msg_appendentries_t* msg) {
    return send_message(raft, node, MSG_APPENDENTRIES, msg);
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

void* server_thread(void* arg) {
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (1) {
        int client_sock = accept(server.server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        int n = recv(client_sock, buffer, MAX_BUFFER_SIZE - 1, 0);
        buffer[n] = '\0';
        
        network_message_t msg;
        deserialize_message(buffer, &msg);
        
        switch (msg.type) {
            case MSG_REQUESTVOTE:
                {
                    msg_requestvote_response_t response;
                    raft_recv_requestvote(server.raft, server.nodes[msg.sender_id - 1], &msg.msg.rv, &response);
                    send_message(server.raft, server.nodes[msg.sender_id - 1], MSG_REQUESTVOTE_RESPONSE, &response);
                }
                break;
            case MSG_APPENDENTRIES:
                {
                    msg_appendentries_response_t response;
                    raft_recv_appendentries(server.raft, server.nodes[msg.sender_id - 1], &msg.msg.ae, &response);
                    send_message(server.raft, server.nodes[msg.sender_id - 1], MSG_APPENDENTRIES_RESPONSE, &response);
                }
                break;
            case MSG_REQUESTVOTE_RESPONSE:
                raft_recv_requestvote_response(server.raft, server.nodes[msg.sender_id - 1], &msg.msg.rvr);
                break;
            case MSG_APPENDENTRIES_RESPONSE:
                raft_recv_appendentries_response(server.raft, server.nodes[msg.sender_id - 1], &msg.msg.aer);
                break;
        }
        
        close(client_sock);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <node_id>\n", argv[0]);
        exit(1);
    }
    
    server.node_id = atoi(argv[1]);
    if (server.node_id < 1 || server.node_id > NUM_NODES) {
        fprintf(stderr, "Invalid node_id. Must be between 1 and %d\n", NUM_NODES);
        exit(1);
    }
    
    raft_cbs_t raft_callbacks = {
        .send_requestvote = __send_requestvote,
        .send_appendentries = __send_appendentries,
        .applylog = __applylog,
        .persist_vote = __persist_vote,
        .persist_term = __persist_term,
        .log = __log
    };
    
    server.raft = raft_new();
    raft_set_callbacks(server.raft, &raft_callbacks, &server);
    
    for (int i = 0; i < NUM_NODES; i++) {
        server.nodes[i] = raft_add_node(server.raft, NULL, i + 1, i + 1 == server.node_id);
    }
    
    struct sockaddr_in server_addr;
    server.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(NODE_IPS[server.node_id - 1]);
    server_addr.sin_port = htons(BASE_PORT + server.node_id - 1);
    
    if (bind(server.server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    if (listen(server.server_socket, 5) < 0) {
        perror("Listen failed");
        exit(1);
    }
    
    pthread_t thread;
    pthread_create(&thread, NULL, server_thread, NULL);
    
    while (1) {
        raft_periodic(server.raft, 100);
        usleep(100000);
    }
    
    return 0;
}