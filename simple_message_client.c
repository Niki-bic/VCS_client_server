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
#include <netdb.h>
#include <simple_message_client_commandline_handling.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>


#define BUF_LEN (1024 * 1024) // noch überlegen wie lang notwendig


void usage(FILE *stream, const char *cmnd, int exitcode);
long handle_reply(char *reply);


int main(const int argc, const char * const *argv) {
    const char *server  = NULL;
    const char *port    = NULL;
    const char *user    = NULL;
    const char *message = NULL;
    const char *img_url = NULL;
    int verbose         = FALSE;

    smc_parsecommandline(argc, argv, &usage, &server, &port, &user, &message, &img_url, &verbose);

    if (verbose != FALSE) {           // -h
        usage(stdout, argv[0], EXIT_SUCCESS);
    }

    struct addrinfo hints;
    struct addrinfo *result  = NULL;
    struct addrinfo *res_ptr = NULL;

    int socket_write = -1;
    int socket_read  = -1;

    FILE *file_write = NULL;
    FILE *file_read  = NULL;

    char reply[BUF_LEN] = {'\0'};

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC;             // egal ob IP4 oder IP6
    hints.ai_socktype = SOCK_STREAM;           // TCP

    int s = getaddrinfo(server, port, &hints, &result);
	if(s != 0){
           fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(s));
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
    
    	errno = 0;
		
        if (close(socket_write) == -1) {
            fprintf(stderr,"Error closing write socket: %s",strerror( errno ));
            exit(EXIT_FAILURE);
        }
    }

    freeaddrinfo(result);
 
    if (res_ptr == NULL) {
        fprintf(stderr,"Error: Client failed to connect");
        // error: client failed to connect
        exit(EXIT_FAILURE);
    }
    
    errno = 0;

    if ((socket_read = dup(socket_write)) == -1) {
        fprintf(stderr,"Error duplicating file discriptor: %s",strerror( errno ));
        exit(EXIT_FAILURE);
    }

    errno = 0;

    if ((file_write = fdopen(socket_write, "w")) == NULL) {

        fprintf(stderr,"Error opening write socket: %s",strerror( errno ));
        // error in fdopen
        exit(EXIT_FAILURE);
    }

    errno = 0;

    if ((file_read = fdopen(socket_read, "r")) == NULL) {

        fprintf(stderr,"Error opening read socket: %s",strerror( errno ));
        // error in fdopen
        exit(EXIT_FAILURE);
    }

    // line-buffering einstellen?
    // setvbuf(file_ptr_write, request, _IOLBF, sizeof(request));
    // setvbuf(file_ptr_read, reply, _IOLBF, sizeof(reply));


    // sending request
    if (img_url == NULL) {
	errno = 0;
        if (fprintf(file_write, "user=%s\n%s\n", user, message) < 0) {
            fprintf(stderr,"Error writing in stream: %s",strerror( errno )); // error fprintf
            exit(EXIT_FAILURE);
        }
    } else { // img_url != NULL
	errno = 0;
        if (fprintf(file_write, "user=%s\nimg=%s\n%s\n", user, img_url, message) < 0) {
            fprintf(stderr,"Error writing in stream: %s",strerror( errno ));
            // error fprintf
            exit(EXIT_FAILURE);
        }
    }

    errno = 0;

    if (fflush(file_write) != 0) { // 
        fprintf(stderr,"Error flushing write stream: %s",strerror( errno ));// error
        exit(EXIT_FAILURE);
    }

    errno = 0;

    if (shutdown(fileno(file_write), SHUT_WR) == -1) { // sendet offenbar EOF
        fprintf(stderr,"Error shutdown write socket failed: %s",strerror( errno ));
        exit(EXIT_FAILURE);
    }
    
    errno = 0;

    if (fclose(file_write) == -1) {
        fprintf(stderr,"Error closing write socket failed: %s",strerror( errno ));
    }

    // einlesen des reply
    int i = 0;
    while (fread(&reply[i * 100], sizeof(reply[0]), 100, file_read) != 0 && i < BUF_LEN) {
        i++;
    }

    long status = handle_reply(reply);
    
    errno = 0;

    if (fclose(file_read) == -1) {
        fprintf(stderr,"Error closing read socket failed: %s",strerror( errno ));
    }

    // close nicht notwendig, da fclose() auch den
    // darunter liegenden Deskriptor schließt (man-page)

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


// eventuell split_input aus simple_message_server_logic abkupfern
long handle_reply(char *reply) {
    long status       = 0;
    unsigned long len = 0;
    char *pointer     = reply;
    FILE *file        = NULL;
    char filename[BUF_LEN] = {'\0'}; // gefällt mir noch nicht ganz


    // search for status
    // dann in einer Schleife file (= filename) und len auslesen, und eine file mit Namen 
    // filename erstellen und den content (d.h. die nächsten len-Bytes) reinschreiben.


    if ((pointer = strstr(reply, "status=")) == NULL) {
	fprintf(stderr,"Error 'status =' not found");
        // error: status= not found
    }
    pointer += 7;                       // strlen("status=") == 7
    errno = 0;
    status = strtol(pointer, NULL, 10); // erro-checking noch einbauen
    
    if (pointer == NULL){
        fprintf (stderr, " number : %lu  invalid  (no digits found, 0 returned)\n", status);
    }else if (errno == ERANGE && status == LONG_MIN){
        fprintf (stderr, " number : %lu  invalid  (underflow occurred)\n", status);
    }else if (errno == ERANGE && status == LONG_MAX){
        fprintf (stderr, " number : %lu  invalid  (overflow occurred)\n", status);
    }else if (errno == EINVAL){
        fprintf (stderr, " number : %lu  invalid  (base contains unsupported value)\n", status);
    }else if (errno != 0 && status == 0){
        fprintf (stderr, " number : %lu  invalid  (unspecified error occurred)\n", status);
    }


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
		errno = 0;
        len = strtol(pointer, NULL, 10); // error-checking noch einbauen
	
		if (pointer == NULL){
            fprintf (stderr, " number : %lu  invalid  (no digits found, 0 returned)\n", len);
		}else if (errno == ERANGE && len == 0){
            fprintf (stderr, " number : %lu  invalid  (underflow occurred)\n", len);
		}else if (errno == ERANGE && len == ULONG_MAX){
            fprintf (stderr, " number : %lu  invalid  (overflow occurred)\n", len);
		}else if (errno == EINVAL){
            fprintf (stderr, " number : %lu  invalid  (base contains unsupported value)\n", len);
    	}else if (errno != 0 && len == 0){
            fprintf (stderr, " number : %lu  invalid  (unspecified error occurred)\n", len);
        }
	
        while (*pointer != '\n') {       // Längenangabe überspringen
            pointer++;
        }
        pointer++;                       // newline-Zeichen schlucken

		errno = 0;
	
        if ((file = fopen(filename, "w")) == NULL) {
            fprintf(stderr,"Error opening file %s: %s",filename ,strerror( errno ));
        }
        
        errno = 0;
        if (fwrite(pointer, sizeof(char), len, file) < len) {
            fprintf(stderr,"Error writing in file %s: %s",filename ,strerror( errno )); // error in fwrite
        }
	
	errno = 0;

        if (fclose(file) == -1) {
            fprintf(stderr,"Error closing file %s: %s",filename ,strerror( errno ));
        }
    } // end while


    return status;
} // end handle_reply()


