#include <errno.h>
#include <netdb.h>
#include "simple_message_client_commandline_handling.h" // mus noch zu <simple...> geändert werden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_LEN 100000


void usage(FILE *stream, const char *cmnd, int exitcode);


int main(const int argc, const char * const*argv) {
    const char *server = NULL;
    const char *port = NULL;
    const char *user = NULL;
    const char *message = NULL;
    const char *img_url = NULL;
    int verbose = 0;

    smc_parsecommandline(argc, argv, &usage, &server, &port, &user, &message, &img_url, &verbose);

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *res_ptr = NULL;

    int socket_fd_write = -1;
    int socket_fd_read = -1;

    FILE *file_ptr_write = NULL;
    FILE *file_ptr_read = NULL;

    char request[BUF_LEN] = {'\0'};
    int reply[BUF_LEN] = {'\0'}; // ws kein char-Buffer sondern besser was anderes, sind ja gemischte Daten

    // eventuell in eine Funktion auslagern und error-checking, sowie strncpy stats strcpy sowie strncat statt strcat
    if (img_url == NULL) {
        strcpy(request, "user=");
        strcat(request, user);
        strcat(request, "\n");
        strcat(request, message);
        strcat(request, "\n"); // unbedingt notwendig
    } else { // img_url != NULL
        strcpy(request, "user=");
        strcat(request, user);
        strcat(request, "\n");
        strcat(request, "img=");
        strcat(request, img_url);
        strcat(request, "\n");
        strcat(request, message);
        strcat(request, "\n"); // unbedingt notwendig
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // egal ob IP4 oder IP6
    hints.ai_socktype = SOCK_STREAM; // TCP

    if (getaddrinfo(server, port, &hints, &result) != 0) {
        // error
    }

    for (res_ptr = result; res_ptr != NULL; res_ptr = res_ptr->ai_next) {
        socket_fd_write = socket(res_ptr->ai_family, res_ptr->ai_socktype, res_ptr->ai_protocol);

        if (socket_fd_write == -1) {
            continue;
        }

        if (connect(socket_fd_write, res_ptr->ai_addr, res_ptr->ai_addrlen) != -1) {
            break;
        }

        close(socket_fd_write);
    }

    freeaddrinfo(result);

    if (res_ptr == NULL) {
        // error: client failed to connect
        exit(EXIT_FAILURE);
    }

    if ((socket_fd_read = dup(socket_fd_write)) == -1) {
        // error
        exit(EXIT_FAILURE);
    }

    if ((file_ptr_write = fdopen(socket_fd_write, "w")) == NULL) {
        // error
        fprintf(stderr, "file_ptr_write ist NULL\n");
        exit(EXIT_FAILURE);
    }

    if ((file_ptr_read = fdopen(socket_fd_read, "rb")) == NULL) {
        // error
        exit(EXIT_FAILURE);
    }

    // line-buffering einstellen
    // setvbuf(file_ptr_write, request, _IOLBF, sizeof(request));
    // setvbuf(file_ptr_read, reply, _IOLBF, sizeof(reply));

    fprintf(file_ptr_write, "%s", request); // oder vl besser fputs()

    if (fflush(file_ptr_write) != 0) { // 
        // error
    }

    if (shutdown(fileno(file_ptr_write), SHUT_WR) == -1) { // sendet offenbar EOF
        // error
    }

    // einlesen des reply
    int c = 0;
    int i = 0;
    while (c != EOF) {
        c = fgetc(file_ptr_read);
        reply[i] = c;
        i++;
    }

    // close() zweimal
    if (close(socket_fd_read) == -1) {
        // error
    }

    if (close(socket_fd_write) == -1) {
        // error
    }

    return EXIT_SUCCESS;
} // end main()


void usage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "usage: %s options\noptions:\n\t\
    -s, --server <server>   full qualified domain name or IP address of the server\n\t\
    -p, --port <port>       well-known port of the server [0..65535]\n\t\
    -u, --user <name>       name of the posting user\n\t\
    -i, --image <URL>       URL pointing to an image of the posting user\n\t\
    -m, --message <message> message to be added to the bulletin board\n\t\
    -v, --verbose           verbose output\n\t\
    -h, --help\n", cmnd);
    exit(exitcode);
} // end usage()