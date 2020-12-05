// todo:
// wie lang muss BUF_LEN sei, d.h. wie lang kann der reply sein?
// error-checking, errno einbauen, Ressourcen im Fehlerfall richtig wergräumen
// d.h. Deskriptoren schließen, File-Pointer schließen etc
// ausprobieren ob -i (bzw -image) funktioniert  -- DONE
// testcases checken
// eventuell modularer aufbauen, auslagern was auch der server braucht
// richtiges schließen der filepointer und socket-filedeskriptoren, welche Reihenfolge, welche müssen überhaupt geschlossen werden
// -h einbauen -- DONE
// das Einlesen des reply noch verbesser (mit Hinblick auf die Testcases)

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <simple_message_client_commandline_handling.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_LEN (1024 * 1024) // noch überlegen wie lang notwendig
#define REPLY_ERROR -3l

const char *cmd = NULL; // globaler Speicher für argv[0]

static long handle_reply(const char *const reply);
static void remove_resources_and_exit(int socket_read, int socket_write, FILE *const file_read,
                                      FILE *const file_write, const int exitcode);
static long strtol_e(const char *const string);
static void usage(FILE *stream, const char *cmnd, int exitcode);

int main(const int argc, const char *const *argv)
{
    cmd = argv[0];

    const char *server = NULL;
    const char *port = NULL;
    const char *user = NULL;
    const char *message = NULL;
    const char *img_url = NULL;
    int verbose = FALSE;

    smc_parsecommandline(argc, argv, &usage, &server, &port, &user, &message, &img_url, &verbose);

    if (verbose == TRUE)
    {
        usage(stdout, argv[0], EXIT_SUCCESS);
    }

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *res_ptr = NULL;

    int socket_write = -1;
    int socket_read = -1;

    FILE *file_write = NULL;
    FILE *file_read = NULL;

    long status = -1;

    char reply[BUF_LEN] = {'\0'};

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // egal ob IP4 oder IP6
    hints.ai_socktype = SOCK_STREAM; // TCP

    int s = getaddrinfo(server, port, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "%s: getaddrinfo: %s\n", cmd, gai_strerror(s));
        return EXIT_FAILURE;
    }

    for (res_ptr = result; res_ptr != NULL; res_ptr = res_ptr->ai_next)
    {
        socket_write = socket(res_ptr->ai_family, res_ptr->ai_socktype, res_ptr->ai_protocol);

        if (socket_write == -1)
        {
            continue;
        }

        if (connect(socket_write, res_ptr->ai_addr, res_ptr->ai_addrlen) != -1)
        {
            break;
        }

        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

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

    // line-buffering einstellen?
    // setvbuf(file_ptr_write, request, _IOLBF, sizeof(request));
    // setvbuf(file_ptr_read, reply, _IOLBF, sizeof(reply));

    errno = 0;

    // sending request
    if (img_url == NULL)
    {
        if (fprintf(file_write, "user=%s\n%s\n", user, message) < 0)
        {
            fprintf(stderr, "%s: Error writing in stream: %s\n", cmd, strerror(errno)); // error fprintf
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
    }

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

    // einlesen des reply // bis hierher kommt er bei TESTCASE=3
    int i = 0;
    while (fread(&reply[i * 100], sizeof(reply[0]), 100, file_read) != 0 && i < BUF_LEN)
    {
        i++;
    }

    status = handle_reply(reply);
    if (status == REPLY_ERROR)
    {
        fprintf(stderr, "%s: Error in handle_reply\n", cmd);
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    errno = 0;

    if (fclose(file_read) == -1)
    {
        fprintf(stderr, "%s: Error closing file failed: %s\n", cmd, strerror(errno));
        remove_resources_and_exit(socket_read, socket_write, file_read, file_write, EXIT_FAILURE);
    }

    // close nicht notwendig, da fclose() auch den
    // darunter liegenden Deskriptor schließt (man-page)

    return (int)status;
} // end main()

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

// eventuell split_input aus simple_message_server_logic abkupfern
static long handle_reply(const char *const reply)
{
    long status = 0;
    long len = 0;
    const char *pointer = (const char *)reply;
    FILE *file = NULL;
    char filename[BUF_LEN] = {'\0'}; // gefällt mir noch nicht ganz

    // search for status
    // dann in einer Schleife file (= filename) und len auslesen, und eine file mit Namen
    // filename erstellen und den content (d.h. die nächsten len-Bytes) reinschreiben.

    if ((pointer = strstr(reply, "status=")) == NULL)
    {
        fprintf(stderr, "%s: Error 'status=' not found\n", cmd);
        return REPLY_ERROR;
    }

    pointer += 7; // strlen("status=") == 7
    status = strtol_e(pointer);
    if (status == REPLY_ERROR)
    {
        fprintf(stderr, "%s: Error: strtol failed\n", cmd);
        return REPLY_ERROR;
    }

    while (TRUE)
    {
        if ((pointer = strstr(pointer, "file=")) == NULL)
        {
            break;
        }

        pointer += 5; // strlen("file=") == 5
        int i = 0;
        memset(filename, 0, BUF_LEN);
        while (*pointer != '\n')
        {
            filename[i] = *pointer;
            i++;
            pointer++;
        }
        pointer++; // newline-Zeichen schlucken

        pointer += 4; // strlen("len=") == 4
        errno = 0;
        len = strtol_e(pointer);
        if (len == REPLY_ERROR)
        {
            fprintf(stderr, "%s: Error: strtol failed\n", cmd);
            return REPLY_ERROR;
        }

        while (*pointer != '\n') // Längenangabe überspringen
        {
            pointer++;
        }
        pointer++; // newline-Zeichen schlucken

        errno = 0;

        if ((file = fopen(filename, "w")) == NULL)
        {
            fprintf(stderr, "%s: Error opening file %s: %s\n", cmd, filename, strerror(errno));
            return REPLY_ERROR;
        }

        errno = 0;

        if (fwrite(pointer, sizeof(char), len, file) < (unsigned long)len)
        {
            fprintf(stderr, "%s: Error writing in file %s: %s\n", cmd, filename, strerror(errno));
            return REPLY_ERROR;
        }

        errno = 0;

        if (fclose(file) == -1)
        {
            fprintf(stderr, "%s: Error closing file %s: %s\n", cmd, filename, strerror(errno));
            return REPLY_ERROR;
        }
    } // end while

    return status;
} // end handle_reply()

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

static void remove_resources_and_exit(int socket_read, int socket_write, FILE *const file_read, FILE *const file_write, const int exitcode)
{
    int exit_status = (int)exitcode;
    if (file_read != NULL)
    {
        if (fclose(file_read) != 0)
        {
            fprintf(stderr, "%s: Error while closing file\n", cmd);
            exit_status = EXIT_FAILURE;
        }
        socket_read = -1;
    }

    if (file_write != NULL)
    {
        if (fclose(file_write) != 0)
        {
            fprintf(stderr, "%s: Error while closing file\n", cmd);
            exit_status = EXIT_FAILURE;
        }
        socket_write = -1;
    }

    if (socket_read != -1)
    {
        if (close(socket_read) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket\n", cmd);
            exit_status = EXIT_FAILURE;
        }
    }

    if (socket_write != -1)
    {
        if (close(socket_write) != 0)
        {
            fprintf(stderr, "%s: Error while closing socket\n", cmd);
            exit_status = EXIT_FAILURE;
        }
    }

    exit(exit_status);
} // end remove_resources
