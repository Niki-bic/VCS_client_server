/**
* @file simple_message_client.c
* VCS Projekt - client server TCP
* 
* @author Binder Patrik         <ic19b030@technikum-wien.at>
* @author Ferchenbauer Nikolaus <ic19b013@technikum-wien.at>
* @date 2020/12/06
*/

#include <limits.h>
#include <simple_message_client_commandline_handling.h>
#include "client_server.h"

/** reply maximum buffer length*/
#define BUF_LEN 4096 // this ensures the default replys are read in at once
/** filename maximum length*/
#define MAX_NAME_LEN 256 // as defined in simple_message_
/** reply error value*/
#define REPLY_ERROR -3l // simple_message_server_logic doesn't use this

/** global storage for argv[0] */
const char *cmd = NULL;

/**
 * \brief handle_reply - handles the reply from server
 * @details this function takes the reply from the server and checks 'status' 
 * and 'len' than it writes the content of the file in the .html and .png file. If 
 * an error occours the client is stopped with EXIT_FAILURE
 *
 * \param file_read - read from stream
 *
 * \return status value or REPLY_ERROR on error
*/

static long handle_reply(FILE *const file_read);

/**
 * \brief remove_resources_and_exit - closes the streams and checks for errors 
 * @details this function closes the file and socket streams and checks if an 
 * error occours, if an error occours the client is stopped with EXIT FAILURE
 *
 * \param socket_read socket read stream
 * \param socket_write socket write stream
 * \param file_read file read stream
 * \param file_write file write stream
 * \param exitcode the desired exitcode
 *
 * \return no return
*/

static void remove_resources_and_exit(int socket_read, int socket_write, FILE *const file_read,
                                      FILE *const file_write, const int exitcode);

/**
 * \brief strol_e - errorchecked strol 
 * @details this function checks  possible errors of strlol and returns the converted long number when sucessful
 *
 * \param string string to be converted
 *
 *
 * \return long value
*/

static long strtol_e(const char *const string);

/**
 * \brief usage - usage for client
 * @details this function shows which parameters can be used in the client program
 *
 * \param stream output stream where the usage is written
 * \param cmnd program name
 * \param exitcode the desired exitcode
 
 * \return no return
*/

static void usage(FILE *stream, const char *cmnd, int exitcode);

/**
 *
 * \brief The main fuction connects the client program to an TCP server.
 * @details This function initializes a connection to a choosen server port,
 * the client is able to send a message and the URL of a picture to the server.
 * 
 * \param argc the number of arguments
 * \param argv command line arguments (including the program name in argv[0])
 *
 * \return status of handle_reply
 */

int main(const int argc, const char *const *argv)
{
    cmd = argv[0];

    const char *server = NULL;
    const char *port = NULL;
    const char *user = NULL;
    const char *message = NULL;
    const char *img_url = NULL;
    int verbose = FALSE;

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *res_ptr = NULL;

    int socket_write = -1;
    int socket_read = -1;

    FILE *file_write = NULL;
    FILE *file_read = NULL;

    long status = -1;

    smc_parsecommandline(argc, argv, &usage, &server, &port, &user, &message, &img_url, &verbose);

    if (verbose == TRUE)
    {
        usage(stdout, argv[0], EXIT_SUCCESS);
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    int s = getaddrinfo(server, port, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "%s: getaddrinfo: %s\n", cmd, gai_strerror(s));
        return EXIT_FAILURE;
    }

    for (res_ptr = result; res_ptr != NULL; res_ptr = res_ptr->ai_next)
    {
        errno = 0;

        if ((socket_write = socket(res_ptr->ai_family, res_ptr->ai_socktype, res_ptr->ai_protocol)) == -1)
        {
            fprintf(stderr, "%s: Error in socket: %s\n", cmd, strerror(errno));
            continue;
        }

        errno = 0;

        if (connect(socket_write, res_ptr->ai_addr, res_ptr->ai_addrlen) != -1)
        {
            break;
        }

        fprintf(stderr, "%s: Error in connect: %s\n", cmd, strerror(errno));

        errno = 0;

        if (close(socket_write) == -1)
        {
            fprintf(stderr, "%s: Error in close: %s", cmd, strerror(errno));
            freeaddrinfo(result);
            return EXIT_FAILURE;
        }
    } // end for

    freeaddrinfo(result);

    if (res_ptr == NULL)
    {
        fprintf(stderr, "%s: Error: Client failed to connect\n", cmd);
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    errno = 0;

    if ((socket_read = dup(socket_write)) == -1)
    {
        fprintf(stderr, "%s: Error duplicating file descriptor: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    errno = 0;

    if ((file_write = fdopen(socket_write, "w")) == NULL)
    {
        fprintf(stderr, "%s: Error opening file: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    errno = 0;

    if ((file_read = fdopen(socket_read, "r")) == NULL)
    {
        fprintf(stderr, "%s: Error opening file: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    errno = 0;

    // sending request
    if (img_url == NULL)
    {
        if (fprintf(file_write, "user=%s\n%s\n", user, message) < 0)
        {
            fprintf(stderr, "%s: Error writing in stream: %s\n", cmd, strerror(errno));
            remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
        }
    }
    else // img_url != NULL
    {
        if (fprintf(file_write, "user=%s\nimg=%s\n%s\n", user, img_url, message) < 0)
        {
            fprintf(stderr, "%s: Error writing in stream: %s\n", cmd, strerror(errno));
            remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
        }
    } // end if (img_url == NULL)

    errno = 0;

    if (fflush(file_write) != 0)
    {
        fprintf(stderr, "%s: Error flushing write stream: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    errno = 0;

    if (shutdown(fileno(file_write), SHUT_WR) == -1)
    {
        fprintf(stderr, "%s: Error shutdown write socket failed: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    errno = 0;

    if (fclose(file_write) == -1)
    {
        fprintf(stderr, "%s: Error closing file failed: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    // reading and handling reply
    if ((status = handle_reply(file_read)) == REPLY_ERROR)
    {
        fprintf(stderr, "%s: Error in handle_reply\n", cmd);
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    (void)fclose(file_read); // no error-checking on a read-only file

    // fclose() also closes the underlying descriptor, so no close() necessary

    return (int)status;
} // end main()

static long handle_reply(FILE *const file_read)
{
    char reply[BUF_LEN] = {'\0'};         // buffer for reply
    char filename[MAX_NAME_LEN] = {'\0'}; // buffer for filename

    long status = 0;
    long len = 0;

    FILE *file = NULL;
    char *pointer = NULL;

    if (fgets(reply, BUF_LEN, file_read) == NULL)
    {
        fprintf(stderr, "%s: Error in fgets\n", cmd);
        return REPLY_ERROR;
    }

    // search for status
    if ((pointer = strstr(reply, "status=")) == NULL)
    {
        fprintf(stderr, "%s: Error 'status=' not found\n", cmd);
        return REPLY_ERROR;
    }

    pointer += 7; // strlen("status=") == 7

    errno = 0;

    status = strtol_e(pointer);

    if (status == REPLY_ERROR)
    {
        fprintf(stderr, "%s: Error: strtol failed\n", cmd);
        return REPLY_ERROR;
    }

    while (TRUE)
    {
        if (fgets(reply, BUF_LEN, file_read) == NULL)
        {
            if (feof(file_read) != 0) // EOF?
            {
                break;
            }
            else // error
            {
                fprintf(stderr, "%s: Error in fgets\n", cmd);
                return REPLY_ERROR;
            }
        }

        if (strncmp(reply, "file=", 5) != 0) // "file=" should be first, if not eat chunks
        {
            continue;
        }

        if ((pointer = strstr(reply, "file=")) == NULL)
        {
            fprintf(stderr, "%s: Error: 'file=' not found\n", cmd);
            return REPLY_ERROR;
        }

        pointer += 5; // strlen("file=") == 5
        memset(filename, '\0', MAX_NAME_LEN);

        int i = 0;
        while (*pointer != '\n')
        {
            filename[i] = *pointer;
            i++;
            pointer++;
        }

        if (fgets(reply, BUF_LEN, file_read) == NULL)
        {
            fprintf(stderr, "%s: Error in fgets\n", cmd);
            return REPLY_ERROR;
        }

        if ((pointer = strstr(reply, "len=")) == NULL)
        {
            fprintf(stderr, "%s: Error: 'len=' not found\n", cmd);
            return REPLY_ERROR;
        }

        pointer += 4; // strlen("len=") == 4

        errno = 0;

        len = strtol_e(pointer);

        if (len == REPLY_ERROR)
        {
            fprintf(stderr, "%s: Error: strtol failed\n", cmd);
            return REPLY_ERROR;
        }

        errno = 0;

        if ((file = fopen(filename, "w")) == NULL)
        {
            fprintf(stderr, "%s: Error opening file %s: %s\n", cmd, filename, strerror(errno));
            return REPLY_ERROR;
        }

        while (len > BUF_LEN)
        {
            errno = 0;

            if (fread(reply, sizeof(char), BUF_LEN, file_read) < BUF_LEN)
            {
                fprintf(stderr, "%s: Error in fread\n", cmd);
                return REPLY_ERROR;
            }

            errno = 0;

            if (fwrite(reply, sizeof(char), BUF_LEN, file) < BUF_LEN)
            {
                fprintf(stderr, "%s: Error writing in file %s: %s\n", cmd, filename, strerror(errno));
                return REPLY_ERROR;
            }

            len -= BUF_LEN;
        } // end while (len > BUF_LEN)

        if (len > 0)
        {
            errno = 0;

            if (fread(reply, sizeof(char), len, file_read) < (unsigned long)len)
            {
                fprintf(stderr, "%s: Error in fread\n", cmd);
                return REPLY_ERROR;
            }

            errno = 0;

            if (fwrite(reply, sizeof(char), len, file) < (unsigned long)len)
            {
                fprintf(stderr, "%s: Error writing in file %s: %s\n", cmd, filename, strerror(errno));
                return REPLY_ERROR;
            }
        } // end if (len > 0)

        errno = 0;

        if (fclose(file) == -1)
        {
            fprintf(stderr, "%s: Error closing file %s: %s\n", cmd, filename, strerror(errno));
            return REPLY_ERROR;
        }
    } // end while (TRUE)

    return status;
} // end handle_reply()

static void remove_resources_and_exit(int socket_read, int socket_write, FILE *const file_read,
                                      FILE *const file_write, const int exitcode)
{
    int exit_status = (int)exitcode;

    if (file_read != NULL)
    {
        errno = 0;
        if (fclose(file_read) != 0)
        {
            fprintf(stderr, "%s: Error while closing file: %s\n", cmd, strerror(errno));
            exit_status = EXIT_FAILURE;
        }
        socket_read = -1;
    }

    if (file_write != NULL)
    {
        errno = 0;
        if (fclose(file_write) != 0)
        {
            fprintf(stderr, "%s: Error while closing file: %s\n", cmd, strerror(errno));
            exit_status = EXIT_FAILURE;
        }
        socket_write = -1;
    }

    if (socket_read != -1)
    {
        errno = 0;
        if (close(socket_read) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket: %s\n", cmd, strerror(errno));
            exit_status = EXIT_FAILURE;
        }
    }

    if (socket_write != -1)
    {
        errno = 0;
        if (close(socket_write) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket: %s\n", cmd, strerror(errno));
            exit_status = EXIT_FAILURE;
        }
    }

    exit(exit_status);
} // end remove_resources()

static long strtol_e(const char *const string)
{
    char *end_ptr;
    errno = 0;
    long number = strtol(string, &end_ptr, 10);

    if (string == NULL)
    {
        fprintf(stderr, "%s: number : %lu invalid (no digits found, 0 returned)\n", cmd, number);
        return REPLY_ERROR;
    }
    else if (errno == ERANGE && number == 0)
    {
        fprintf(stderr, "%s: number : %lu invalid (underflow occurred)\n", cmd, number);
        return REPLY_ERROR;
    }
    else if (errno == ERANGE && number == LONG_MAX)
    {
        fprintf(stderr, "%s: number : %lu invalid (overflow occurred)\n", cmd, number);
        return REPLY_ERROR;
    }
    else if (errno == EINVAL)
    {
        fprintf(stderr, "%s: number : %lu invalid (base contains unsupported value)\n", cmd, number);
        return REPLY_ERROR;
    }
    else if (errno != 0 && number == 0)
    {
        fprintf(stderr, "%s: number : %lu invalid (unspecified error occurred)\n", cmd, number);
        return REPLY_ERROR;
    }

    return number;
} // end strtol_e()

static void usage(FILE *stream, const char *cmnd, int exitcode)
{
    fprintf(stream, "usage: %s options\noptions:\n\t\
    -s, --server <server>   full qualified domain name or IP address of the server\n\t\
    -p, --port <port>       well-known port of the server [0..65535]\n\t\
    -u, --user <name>       name of the posting user\n\t\
    -i, --image <URL>       URL pointing to an image of the posting user\n\t\
    -m, --message <message> message to be added to the bulletin board\n\t\
    -v, --verbose           verbose output\n\t\
    -h, --help\n",
            cmnd);

    exit(exitcode);
} // end usage()
