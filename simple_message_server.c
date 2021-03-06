/**
* @file simple_message_server.c
* VCS Projekt - client server TCP
* 
* @author Binder Patrik         <ic19b030@technikum-wien.at>
* @author Ferchenbauer Nikolaus <ic19b013@technikum-wien.at>
* @date 2020/12/06
*/

#include "client_server.h"
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>

#define BACKLOG 10
/** defines FALSE 0*/
#define FALSE 0
/** defines TRUE 1*/
#define TRUE 1

/**global storage for argv[0] */
const char *cmd = NULL;

/**
 * \brief close_e - closes socket and checks for errors 
 * @details this function closes the socket and checks if an 
 * error occours, if an error occours the server is stopped with EXIT FAILURE
 *
 * \param socket socket stream
 *
 * \return no return
*/

static void close_e(const int socket);

/**
 * \brief dup2_e - duplicates file descriptor and checks for errors 
 * @details this function duplicates the file descriptor and checks if an 
 * error occours, if an error occours the streams are closed and the server ist stopped with EXIT_FAILURE
 *
 * \param socket socket stream
 *
 * \return no return
*/

static void dup2_e(int socket_old, int socket_new);

/**
 * \brief handle_sigchild - callback-function for signal
 * @details this function is called on termination of the child-process
 * (which implements the server-logic) to reap the exit-status
 *
 * \param s
 *
 * \return no return
*/

static void handle_sigchild(int s);

/**
 * \brief remove_resources_and_exit - closes the socket and checks for errors 
 * @details this function closes the connect and listen socket streams and checks if an 
 * error occours, if an error occours the server is stopped with EXIT FAILURE
 *
 * \param socket_listen listening socket
 * \param socket_connect connected socket
 * \param exitcode 
 *
 * \return no return
*/

static void remove_resources_and_exit(const int socket_listen,
                                      const int socket_connect, const int exitcode);

/**
 * \brief usage - usage for server
 * @details this function shows which parameters can be used in the server program
 *
 * \param stream output stream where the usage is writen
 * \param cmnd argv[0]
 * \param exitcode
 
 * \return no return
*/

static void usage(FILE *const stream, const char *cmnd, const int exitcode);

/**
 *
 * \brief The main function starts an TCP server.
 * @details This function initializes a server with a choosen server port,
 * clients are able to connect to this server and are able to communicate with it 
 * 
 * \param argc the number of arguments
 * \param argv command line arguments (including the program name in argv[0])
 *
 * \return no return
 */

int main(const int argc, const char *const *argv)
{
    cmd = argv[0];

    int c = -1;  // "char" for getopt_long
    int yes = 1; // for setsockopt
    int pid = -1;

    int socket_listen = -1;
    int socket_connect = -1;

    char *port = NULL;

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *res_ptr = NULL;

    struct sigaction sa;

    struct option long_options[] = {
        {"port", 1, NULL, 'p'},
        {"help", 0, NULL, 'h'},
        {0, 0, 0, 0}};

    while ((c = getopt_long(argc, (char *const *)argv, "p:h", long_options, NULL)) != -1)
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
            break;
        }
    }

    if ((optind != argc) || (port == NULL))
    {
        usage(stderr, argv[0], EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // set IP automatically

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

        errno = 0;

        if (setsockopt(socket_listen, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        {
            fprintf(stderr, "%s: Error in setsockopt: %s\n", cmd, strerror(errno));
        }

        errno = 0;

        if (bind(socket_listen, res_ptr->ai_addr, res_ptr->ai_addrlen) == -1)
        {
            fprintf(stderr, "%s: Error in bind: %s\n", cmd, strerror(errno));

            errno = 0;

            if (close(socket_listen) == -1)
            {
                fprintf(stderr, "%s: Error in close: %s\n", cmd, strerror(errno));
                freeaddrinfo(result);
                return EXIT_FAILURE;
            }
            continue;
        }

        if (listen(socket_listen, BACKLOG) != -1)
        {
            break;
        }

        errno = 0;

        if (close(socket_listen) == -1)
        {
            fprintf(stderr, "%s: Error in close: %s\n", cmd, strerror(errno));
            freeaddrinfo(result);
            return EXIT_FAILURE;
        }
    }

    freeaddrinfo(result); // don't need it anymore

    if (res_ptr == NULL)
    {
        fprintf(stderr, "%s: Error no proper address-information found\n", cmd);
        remove_resources_and_exit(socket_listen, -1, EXIT_FAILURE);
    }

    errno = 0;

    sa.sa_handler = handle_sigchild; // reap all dead processes
    if (sigemptyset(&sa.sa_mask) == -1)
    {
        fprintf(stderr, "%s: Error in sigemptyset: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_listen, -1, EXIT_FAILURE);
    }

    sa.sa_flags = SA_RESTART;

    errno = 0;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) // SIGCHILD: child stopped or terminated
    {
        fprintf(stderr, "%s: Error in sigaction: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_listen, -1, EXIT_FAILURE);
    }

    while (TRUE)
    {
        errno = 0;

        if ((socket_connect = accept(socket_listen, res_ptr->ai_addr, &res_ptr->ai_addrlen)) == -1)
        {
            if (errno == EINTR) // don't abort on SIGCHILD
            {
                continue; // not really necessary, just for clarity
            }
            else
            {
                fprintf(stderr, "%s: Error in accepting connection: %s\n", cmd, strerror(errno));
                remove_resources_and_exit(socket_listen, -1, EXIT_FAILURE);
            }

            continue;
        }

        errno = 0;

        switch (pid = fork())
        {
        case -1: // error in fork()
            fprintf(stderr, "%s: Error in fork: %s\n", cmd, strerror(errno));
            remove_resources_and_exit(socket_listen, socket_connect, EXIT_FAILURE);
            break; // not necessary but emphasizes the end of case 1

        case 0: // child
            close_e(socket_listen);

            // redirect stdin and stdout to socket_connect
            dup2_e(socket_connect, STDOUT_FILENO);
            dup2_e(socket_connect, STDIN_FILENO);

            close_e(socket_connect);

            execlp("simple_message_server_logic", "simple_message_server_logic", (char *)NULL);
            // here only if an error in execlp occurs

            fprintf(stderr, "%s: Error in execlp\n", cmd);
            remove_resources_and_exit(socket_listen, socket_connect, EXIT_FAILURE);
            break; // not necessary but emphasizes the end of case 0

        default: // parent
            close_e(socket_connect);
            break;
        } // end switch

    } // end while

} // end main()

static void close_e(const int socket)
{
    if (socket != -1)
    {
        errno = 0;
        if (close(socket) == -1)
        {
            fprintf(stderr, "%s: Error while closing socket %d: %s\n", cmd, socket, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
} // end close_e()

static void dup2_e(int socket_old, int socket_new)
{
    errno = 0;
    if (dup2(socket_old, socket_new) == -1)
    {
        fprintf(stderr, "%s: Error in dup2: %s\n", cmd, strerror(errno));
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

static void remove_resources_and_exit(const int socket_listen,
                                      const int socket_connect, const int exitcode)
{
    if (socket_listen != -1)
    {
        errno = 0;
        if (close(socket_listen) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket %d: %s\n", cmd, socket_listen, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if (socket_connect != -1)
    {
        errno = 0;
        if (close(socket_connect) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket %d: %s\n", cmd, socket_connect, strerror(errno));
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
