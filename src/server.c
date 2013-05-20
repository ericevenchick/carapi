#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>

#include "canstore.h"

#define REQ_BUFFER_SIZE 1000
#define RESP_BUFFER_SIZE 1000
#define MAX_BACKLOG 50

canstore_t canstore;

// set to 1 to stop receive loop
static char stop_server = 0;

void do_stop_server(int dummy)
{
    stop_server = 1;
    syslog(LOG_INFO, "caught SIGINT, shutting down");
}

int main(int argc, char *argv[])
{
    int host_sock;

    // enable logging
    openlog("carapi", LOG_PERROR | LOG_PID, LOG_USER);

    // enable signal handling
    if (setup_signals() < 0) {
        exit(EXIT_FAILURE);
    }

    // set up canstore
    canstore = canstore_init("can0");
    canstore_start(canstore);

    // create and bind the host socket
    host_sock = get_host_socket("127.0.0.1", 1234);
    if (host_sock < 0) {
        exit(EXIT_FAILURE);
    }

    // start the receive loop
    if (recv_loop(host_sock) < 0) {
        exit(EXIT_FAILURE);
    }

    // close the host socket and exit
    close(host_sock);
    exit(EXIT_SUCCESS);

    return 0;
}

int setup_signals() {
    struct sigaction sigint_action;

    // setup handling of SIGINT
    sigint_action.sa_handler = do_stop_server;
    sigint_action.sa_flags = SA_NODEFER;
    if (sigaction(SIGINT, &sigint_action, NULL) < 0) {
        perror("sigaction");
        return -1;
    }
}

int get_host_socket(char *addr, int port)
{
    int host_sock;
    struct sockaddr_in host_addr;
    int optval;
    socklen_t optlen = sizeof(int);

    // create host socket
    host_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (host_sock < 0) {
        perror("socket");
        return -1;
    }

    // zero the address struct
    memset(&host_addr, 0, sizeof(struct sockaddr_in));
    // set the socket family to IPv4
    host_addr.sin_family = AF_INET;
    // TODO: set address front string
    host_addr.sin_addr.s_addr = 0;
    // fill remainder of address with zeros
    memset(&(host_addr.sin_zero), '\0', 8);
    // set the port
    host_addr.sin_port = htons(port);

    optval = 1;
    setsockopt(host_sock, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);

    // bind the socket to the address
    if (bind(host_sock, (struct sockaddr *) &host_addr,
        sizeof(struct sockaddr)) < 0) {
            perror("bind");
            return -1;
    }

    return host_sock;
}

int recv_loop(int host_sock)
{
    int client_sock;
    struct sockaddr_in client_addr;
    socklen_t sin_size;
    char req_buffer[REQ_BUFFER_SIZE];
    char resp_buffer[RESP_BUFFER_SIZE];
    char *client_ip;

    sin_size = sizeof(struct sockaddr_in);

    if (listen(host_sock, MAX_BACKLOG) < 0) {
        perror("listen");
        return -1;
    }

    while (!stop_server) {
        client_sock = accept(host_sock, (struct sockaddr *) &client_addr,
            &sin_size);
        if (client_sock == -1) {
            perror("accept");
            return -1;
        }
        client_ip = inet_ntoa(client_addr.sin_addr);
        syslog(LOG_INFO, "accepted connection from %s", client_ip);

        memset(&req_buffer, '\0', sizeof(req_buffer));
        recv(client_sock, &req_buffer, sizeof(req_buffer), 0);
        handle_request(req_buffer, &resp_buffer);

        write(client_sock, resp_buffer, sizeof(resp_buffer));
        close(client_sock);
    }

    return 0;
}

int handle_request(char *req_buffer, char *resp_buffer)
{
    char req_type[10];
    char req_arg[100];

    sscanf(req_buffer, "%10s %100s", req_type, req_arg);

    syslog(LOG_INFO, "got request, type=%s arg=%s", &req_type, &req_arg);

    memset(resp_buffer, '\0', RESP_BUFFER_SIZE);

    if (strcmp("GET", req_type) == 0) {
        handle_get_request(req_buffer, resp_buffer, req_arg);
    } else {
        handle_invalid_request(req_buffer, resp_buffer, req_arg);
    }
}

int handle_get_request(char *req_buffer, char *resp_buffer, char *req_arg)
{
    char resp_payload[1000];
    double data;

    // skip over leading '/' character in request
    req_arg++;

    syslog(LOG_INFO, "GET request for key=%s", req_arg);

    data = canstore_get(canstore, 1);

    sprintf(resp_payload, "{status: 'success', data: {id: '%f'}}\n", data);
    sprintf(resp_buffer, "HTTP/1.1 200 OK\nContent-Type: application/json; charset=utf-8\nContent-Length: %d\n\n%s", strlen(resp_payload), resp_payload);

    return 0;
}

int handle_invalid_request(char *req_buffer, char *resp_buffer, char *req_arg)
{
    sprintf(resp_buffer, "HTTP/1.1 501 Not Implemented\n");
    return 0;
}
