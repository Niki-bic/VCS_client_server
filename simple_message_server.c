/* 
Verständnisfrage an die Lektoren:
Wenn sms_logic von stdin liest und angenommen es kommen in kurzer Zeit hintereinander 
2 Requests von Clients rein, wie kann man sicherstellen, dass der 2. Request nicht den
ersten überschreibt?
*/

#include <signal.h>
#include <sys/wait.h>
#include "client_server.h"

#define BACKLOG 5 // wie groß tatsächlich?
#define TRUE 1
#define FALSE 0

const char *cmd = NULL; // globaler Speicher für argv[0] damit es nicht
                        // jeder Funktion mitgegeben werden muss

static void close_e(const int socket);              // Error-checking-Wrapper um close()
static void dup2_e(int socket_old, int socket_new); // Error-checking-Wrapper um dup2()
static void handle_sigchild(int s);
static void remove_resources_and_exit(const int socket_listen,
                                      const int socket_connect, const int exitcode);
static void usage(FILE *const stream, const char *cmnd, const int exitcode);

int main(const int argc, const char *const *argv)
{
    cmd = argv[0];

    int c = -1;
    char *port = NULL;

    while ((c = getopt(argc, (char *const *)argv, "p:h")) != -1)
    {
        switch (c)
        {
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
    struct addrinfo *result = NULL;
    struct addrinfo *res_ptr = NULL;

    struct sigaction sa;

    int socket_listen = -1;
    int socket_connect = -1;

    int pid = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // IP automatisch ausfüllen

    if (getaddrinfo(NULL, port, &hints, &result) != 0)
    {
        fprintf(stderr, "%s: Error in getaddrinfo\n", cmd);
        return EXIT_FAILURE;
    }

    for (res_ptr = result; res_ptr != NULL; res_ptr = res_ptr->ai_next)
    {
        socket_listen = socket(res_ptr->ai_family, res_ptr->ai_socktype, res_ptr->ai_protocol);

        if (socket_listen == -1)
        {
            continue;
        }

        if (bind(socket_listen, res_ptr->ai_addr, res_ptr->ai_addrlen) == -1)
        {
            close_e(socket_listen);
            continue;
        }

        if (listen(socket_listen, BACKLOG) != -1)
        {
            break;
        }

        close_e(socket_listen);
    }

    freeaddrinfo(result);

    if (res_ptr == NULL)
    {
        fprintf(stderr, "%s: Error no proper address-information found\n", cmd);
        remove_resources_and_exit(socket_listen, -1, EXIT_FAILURE);
    }

    sa.sa_handler = handle_sigchild; // reap all dead processes
    if (sigemptyset(&sa.sa_mask) == -1)
    {
        fprintf(stderr, "%s: Error in sigemptyset\n", cmd);
        remove_resources_and_exit(socket_listen, -1, EXIT_FAILURE);
    }

    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        fprintf(stderr, "%s: Error in sigaction\n", cmd);
        remove_resources_and_exit(socket_listen, -1, EXIT_FAILURE);
    }

    while (TRUE)
    {
        errno = 0;
        if ((socket_connect = accept(socket_listen, res_ptr->ai_addr, &res_ptr->ai_addrlen)) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                fprintf(stderr, "%s: Error in accepting connection\n", cmd);
                remove_resources_and_exit(socket_listen, -1, EXIT_FAILURE);
            }

            continue;
        }

        switch (pid = fork())
        {
        case -1: // fehler bei fork()
            fprintf(stderr, "%s: Error in fork\n", cmd);
            remove_resources_and_exit(socket_listen, socket_connect, EXIT_FAILURE);
            break; // eigentlich nicht notwendig

        case 0: // child
            close_e(socket_listen);

            // socket duplizieren und auf stdin und stdout umlenken
            dup2_e(socket_connect, STDOUT_FILENO);
            dup2_e(socket_connect, STDIN_FILENO);

            close_e(socket_connect);

            execlp("simple_message_server_logic", "simple_message_server_logic", (char *)NULL);
            // error, hier nur wenn execlp versagt

            remove_resources_and_exit(socket_listen, socket_connect, EXIT_FAILURE);
            break; // eigentliche nicht notwendig

        default: // parent
            close_e(socket_connect);
            break;
        } // switch

    } // end while

} // end main()

static void close_e(const int socket)
{
    if (socket != -1)
    {
        if (close(socket) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket %d\n", cmd, socket);
            exit(EXIT_FAILURE);
        }
    }
} // end close_e()

static void dup2_e(int socket_old, int socket_new)
{
    if (dup2(socket_old, socket_new) == -1)
    {
        fprintf(stderr, "%s: Error in dup2\n", cmd);
        remove_resources_and_exit(socket_old, -1, EXIT_FAILURE);
    }
} // end dup2_e()

static void handle_sigchild(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while ((s = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        ;
    }

    errno = saved_errno;
} // end handle_sigchild()

static void remove_resources_and_exit(const int socket_listen, const int socket_connect, const int exitcode)
{
    if (socket_listen != -1)
    {
        if (close(socket_listen) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket %d\n", cmd, socket_listen);
            exit(EXIT_FAILURE);
        }
    }

    if (socket_connect != -1)
    {
        if (close(socket_connect) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket %d\n", cmd, socket_connect);
            exit(EXIT_FAILURE);
        }
    }

    exit(exitcode);
} // end remove_resources_and_exit()

static void usage(FILE *const stream, const char *cmnd, const int exitcode)
{
    fprintf(stream, "usage: %s options\noptions:\n\t\
    -p, --port <port>\n\t\
    -h, --help\n",
            cmnd);
    exit(exitcode);
} // end usage()
