// todo:
// wie lang muss BUF_LEN sei, d.h. wie lang kann der reply sein?
// error-checking, errno einbauen, Ressourcen im Fehlerfall richtig wergräumen
// d.h. Deskriptoren schließen, File-Pointer schließen etc
// ausprobieren ob -i (bzw -image) funktioniert  -- DONE
// testcases checken
// eventuell modularer aufbauen, auslagern was auch der server braucht
// richtiges schließen der filepointer und socket-filedeskriptoren, welche Reihenfolge, welche müssen überhautp geschossen werden
// -h einbauen -- DONE


#include <errno.h>
#include <netdb.h>
#include <simple_message_client_commandline_handling.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_LEN 100000 // noch überlegen wie lang notwendig


void usage(FILE *stream, const char *cmnd, int exitcode);
long handle_reply(char *reply);


int main(const int argc, const char * const *argv) {
    const char *server = NULL;
    const char *port = NULL;
    const char *user = NULL;
    const char *message = NULL;
    const char *img_url = NULL;
    int verbose = 0;

    smc_parsecommandline(argc, argv, &usage, &server, &port, &user, &message, &img_url, &verbose);

    if (verbose == 1) { // -h
        usage(stdout, argv[0], 0);
    }

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *res_ptr = NULL;

    int socket_write = -1;
    int socket_read = -1;

    FILE *file_write = NULL;
    FILE *file_read = NULL;

    int request_len = 0;
    char *request = NULL;
    char reply[BUF_LEN] = {'\0'};

    request_len = strlen("user=") + strlen(user) + 1 + strlen(message) + 2;

    // eventuell in eine Funktion auslagern und error-checking, sowie strncpy stats strcpy sowie strncat statt strcat
    if (img_url == NULL) {
        request = (char *) calloc(request_len, sizeof(char));

        if (request == NULL) {
            // error
            exit(EXIT_FAILURE);
        }

        strcpy(request, "user=");
        strcat(request, user);
        strcat(request, "\n");
        strcat(request, message);
        strcat(request, "\n"); // unbedingt notwendig
    } else { // img_url != NULL
        request_len += strlen("img=") + strlen(img_url) + 1;
        request = (char *) calloc(request_len, sizeof(char));

        if (request == NULL) {
            // error
            exit(EXIT_FAILURE);
        }

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
        exit(EXIT_FAILURE);
    }

    for (res_ptr = result; res_ptr != NULL; res_ptr = res_ptr->ai_next) {
        socket_write = socket(res_ptr->ai_family, res_ptr->ai_socktype, res_ptr->ai_protocol);

        if (socket_write == -1) {
            continue;
        }

        if (connect(socket_write, res_ptr->ai_addr, res_ptr->ai_addrlen) != -1) {
            break;
        }

        if (close(socket_write) == -1) {
            // error
            exit(EXIT_FAILURE);
        }
    }

    freeaddrinfo(result);

    if (res_ptr == NULL) {
        // error: client failed to connect
        exit(EXIT_FAILURE);
    }

    if ((socket_read = dup(socket_write)) == -1) {
        // error in dup()
        exit(EXIT_FAILURE);
    }

    if ((file_write = fdopen(socket_write, "w")) == NULL) {
        // error in fdopen
        exit(EXIT_FAILURE);
    }

    if ((file_read = fdopen(socket_read, "r")) == NULL) {
        // error in fdopen
        exit(EXIT_FAILURE);
    }

    // line-buffering einstellen?
    // setvbuf(file_ptr_write, request, _IOLBF, sizeof(request));
    // setvbuf(file_ptr_read, reply, _IOLBF, sizeof(reply));

    if (fprintf(file_write, "%s", request) < 0) {
        // error fprintf
        exit(EXIT_FAILURE);
    }

    if (fflush(file_write) != 0) { // 
        // error
        exit(EXIT_FAILURE);
    }

    free(request);

    if (shutdown(fileno(file_write), SHUT_WR) == -1) { // sendet offenbar EOF
        // error
        exit(EXIT_FAILURE);
    }

    // einlesen des reply
    int i = 0;
    while (fread(&reply[i * 100], sizeof(reply[0]), 100, file_read) != 0) {
        i++;
    }

    long status = handle_reply(reply);

    if (fclose(file_read) == -1) {
        // error        
    }
    
    if (fclose(file_write) == -1) {
        // error
    }

    // close() zweimal?
    if (close(socket_read) == -1) {
        // error
    }

    if (close(socket_write) == -1) {
        // error
    }

    return status;
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


long handle_reply(char *reply) {
    long status = 0;
    unsigned long len = 0;
    char *pointer = reply;
    char filename[BUF_LEN] = {'\0'}; // gefällt mir noch nicht ganz
    FILE *file = NULL;

    // search for status
    // dann in einer Schleife file (= filename) und len auslesen, und eine file mit Namen 
    // filename erstellen und den content (d.h. die nächsten len-Bytes) reinschreiben.


    if ((pointer = strstr(reply, "status=")) == NULL) {
        // error: status= not found
    }
    pointer += 7;                       // strlen("status=") == 7
    status = strtol(pointer, NULL, 10); // erro-checking noch einbauen



    while (TRUE) {
        if ((pointer = strstr(pointer, "file=")) == NULL) {
            // error: no file=
            break; // das ist wichtig!
        }
        pointer += 5; // strlen("file=") == 5
        int i = 0;
        memset(filename, 0, BUF_LEN);
        while(*pointer != '\n') {
            filename[i] = *pointer;
            i++;
            pointer++;
        }
        pointer++;                       // newline-Zeichen schlucken


        pointer += 4;                    // strlen("len=") == 4
        len = strtol(pointer, NULL, 10); // error-checking noch einbauen

        while (*pointer != '\n') {       // Längenangabe überspringen
            pointer++;
        }
        pointer++;                       // newline-Zeichen schlucken


        if ((file = fopen(filename, "w")) == NULL) {
            // error in fopen
        }
      
        if (fwrite(pointer, sizeof(char), len, file) < len) {
            // error in fwrite
        }

        if (fclose(file) == -1) {
            // error in fclose
        }
    } // end while


    return status;
} // end handle_reply()

