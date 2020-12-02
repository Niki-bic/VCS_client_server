// was erwartet sich die simple_message_server_logic als Argumente?
#include <errno.h>
#include <netdb.h>
#include "simple_message_client_commandline_handling.h" // mus noch zu <simple...> geändert werden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLOG 5
#define REQ_LEN 100000


void usage(FILE *stream, const char *cmnd, int exitcode);


int main(const int argc, const char * const *argv) {
    const char *server = NULL;
    const char *port = NULL;
    const char *user = NULL;
    const char *message = NULL;
    const char *img_url = NULL;
    int verbose = 0;

    smc_parsecommandline(argc, argv, &usage, &server, &port, &user, &message, &img_url, &verbose);

    if (verbose != 0) {           // -h
        usage(stdout, argv[0], 0);
    }

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *res_ptr = NULL;

    int socket_listen = -1;
    int socket_connect = -1;

    FILE *file_write = NULL;
    FILE *file_read = NULL;

    int pid = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;                 // Server muss nur IP4 können
    hints.ai_socktype = SOCK_STREAM;           // TCP
    hints.ai_flags = AI_PASSIVE;               // IP automatisch ausfüllen

    if (getaddrinfo(NULL, port, &hints, &result) != 0) {
        // error
        exit(EXIT_FAILURE);
    }

    for (res_ptr = result; res_ptr != NULL; res_ptr = res_ptr->ai_next) {
        socket_listen = socket(res_ptr->ai_family, res_ptr->ai_socktype, res_ptr->ai_protocol);

        if (socket_listen == -1) {
            continue;
        }

        if (bind(socket_listen, res_ptr->ai_addr, res_ptr->ai_addrlen) != -1) {
            break;
        }

        if (close(socket_listen) == -1) {
            // error
            exit(EXIT_FAILURE);
        }
    }

    freeaddrinfo(result);

    if (res_ptr == NULL) {
        // error: client failed to connect
        exit(EXIT_FAILURE);
    }

    if (listen(socket_listen, BACKLOG) == -1) { // möglicherweise in die for-Schleife
        // error
    }

    // request einlesen und aufbereiten noch machen

    while (TRUE) {
        if ((socket_connect = accept(socket_listen, res_ptr->ai_addr, res_ptr->ai_addrlen)) == -1) {
            // error
            continue;
        }

	    switch (pid = fork()) {  
            case -1:               // fehler bei fork()                                    


                break;
            case 0:                // child                                   
                if (close(socket_listen) == -1) {
                    // error
                }
                // socket duplizieren und auf stdin und stdout umlenken
                // request des client auf stdout schreiben (oder doch auf stdin schreiben?, wenn doch von dort vom sms_logic gelesen wird)
                execlp("simple_message_server_logcic", (char *) NULL);
                // error, hier nur wenn execlp versagt
                break;
            default:               // parent                               
                if (close(socket_connect) == -1) {
                    // error
                }

                // waitpid eventuell hier?

                break;
        }



    } // end while




    return 0;
} // end main()


void usage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "usage: %s options\noptions:\n\t\
    -p, --port <port>\n\t\
    -h, --help\n", cmnd);
    exit(exitcode);
} // end usage()
