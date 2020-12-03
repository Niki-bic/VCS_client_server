/* 
Verständnisfrage an die Lektoren:
Wenn sms_logic von stdin liest und angenommen es kommen in kurzer Zeit hintereinander 
2 Requests von Clients rein, wie kann man sicherstellen, dass der 2. Request nicht den
ersten überschreibt?
*/

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BACKLOG 5          // wie groß tatsächlich?
#define TRUE 1
#define FALSE 0


void usage(FILE *stream, const char *cmnd, int exitcode);
void sigchild_handler(int s);


int main(const int argc, const char * const *argv) {
    int c = -1;
    char *port  = NULL;

    while ((c = getopt(argc, (char * const *) argv, "p:h")) != -1) {
        switch (c) {
            case 'p':
                port = optarg;
                break;

            case 'h':
                usage(stdout, argv[0], EXIT_SUCCESS);
                break;

            case '?':
            default:
                usage(stderr, argv[0], EXIT_FAILURE);
        }
    }

    struct addrinfo hints;
    struct addrinfo *result  = NULL;
    struct addrinfo *res_ptr = NULL;

    struct sigaction sa;

    int socket_listen  = -1;
    int socket_connect = -1;

    int pid = -1;


    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;             // TCP
    hints.ai_flags    = AI_PASSIVE;              // IP automatisch ausfüllen

    if (getaddrinfo(NULL, port, &hints, &result) != 0) {
        // error
        exit(EXIT_FAILURE);
    }


    for (res_ptr = result; res_ptr != NULL; res_ptr = res_ptr->ai_next) {
        socket_listen = socket(res_ptr->ai_family, res_ptr->ai_socktype, res_ptr->ai_protocol);

        if (socket_listen == -1) {
            continue;
        }

        if (bind(socket_listen, res_ptr->ai_addr, res_ptr->ai_addrlen) == -1) {
            if (close(socket_listen) == -1) {
                // error
                exit(EXIT_FAILURE);
            }            

            continue;
        }

        if (listen(socket_listen, BACKLOG) != -1) {
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


    sa.sa_handler = sigchild_handler;      // reap all dead processes
    if (sigemptyset(&sa.sa_mask) == -1) {
        // error
    }
    
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }


    while (TRUE) {
        errno = 0;
        if ((socket_connect = accept(socket_listen, res_ptr->ai_addr, &res_ptr->ai_addrlen)) == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                // error
            }

            continue;
        }

	    switch (pid = fork()) {  
            case -1:                                           // fehler bei fork()                                    
                // error
                exit(EXIT_FAILURE);
                break;

            case 0:                                            // child                                   
                if (close(socket_listen) == -1) {
                    // error
                }

                // socket duplizieren und auf stdin und stdout umlenken
                if (dup2(socket_connect, STDOUT_FILENO) == -1) {
                    // error
                }

                if (dup2(socket_connect, STDIN_FILENO) == -1) {
                    // error
                }

                if (close(socket_connect) == -1) {
                    // error
                }                

                execlp("simple_message_server_logic", "simple_message_server_logic", (char *) NULL);
                // error, hier nur wenn execlp versagt

                _exit(EXIT_FAILURE);
                break;

            default:                                           // parent                               
                if (close(socket_connect) == -1) {
                    // error
                }

                break;
        }

    } // end while

} // end main()


void usage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "usage: %s options\noptions:\n\t\
    -p, --port <port>\n\t\
    -h, --help\n", cmnd);
    exit(exitcode);
} // end usage()


void sigchild_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while ((s = waitpid(-1, NULL, WNOHANG)) > 0) {
        ;
    }

    errno = saved_errno;
} // end sig_child