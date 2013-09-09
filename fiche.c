/*
Fiche - Command line pastebin for sharing terminal output.

-------------------------------------------------------------------------------

License: MIT (http://www.opensource.org/licenses/mit-license.php)
Repository: https://github.com/solusipse/fiche/
Live example: http://code.solusipse.net/

-------------------------------------------------------------------------------

usage: fiche [-bdpqs].
             [-d domain] [-p port] [-s slug_size]
             [-o output directory] [-b buffer_size]
             [-l log file] [-q queue_size]

Compile with Makefile or manually with -O2 and -pthread flags.
To install use `make install` command.

Use netcat to push text - example:

$ cat fiche.c | nc localhost 9999

-------------------------------------------------------------------------------
*/

#include "fiche.h"

int main(int argc, char **argv)
{
    time_seed = time(0);

    parse_parameters(argc, argv);
    if (BASEDIR == NULL)
        set_basedir();
    
    startup_message();

    int listen_socket, address_lenght, optval = 1;
    struct sockaddr_in server_address;

    listen_socket = create_socket();
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    server_address = set_address(server_address);
    bind_to_port(listen_socket, server_address);

    while (1) perform_connection(listen_socket);
}

void *thread_connection(void *args)
{
    int connection_socket = ((struct thread_arguments *) args ) -> connection_socket;
    struct sockaddr_in client_address = ((struct thread_arguments *) args ) -> client_address;

    int n;
    char buffer[BUFSIZE];
    bzero(buffer, BUFSIZE);
    int status = recv(connection_socket, buffer, BUFSIZE, 0);

    if (status != -1)
    {
        char slug[SLUG_SIZE];
        generate_url(buffer, slug);

        get_client_address(client_address, slug);

        char response[strlen(slug) + strlen(DOMAIN) + 2];
        strcpy(response, DOMAIN);
        strcat(response, slug);
        strcat(response, "/\n");
        write(connection_socket, response, strlen(response));
    }
    else
    {
        get_client_address(client_address, NULL);
        printf("Invalid connection.\n");
        write(connection_socket, "Use netcat.\n", 13);
    }
    
    close(connection_socket);
    pthread_exit(NULL);
}

void perform_connection(int listen_socket)
{
    void *status = 0;
    pthread_t thread_id;
    struct sockaddr_in client_address;
    
    int address_lenght = sizeof(client_address);
    int connection_socket = accept(listen_socket, (struct sockaddr *) &client_address, (void *) &address_lenght);

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    if (setsockopt (connection_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        error();
    if (setsockopt (connection_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        error();

    struct thread_arguments arguments;
    arguments.connection_socket = connection_socket;
    arguments.client_address = client_address;

    if (pthread_create(&thread_id, NULL, &thread_connection, &arguments) != 0)
        error();
    else
        pthread_detach(thread_id);

}

void display_date()
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    printf("%s", asctime(timeinfo));
}

void get_client_address(struct sockaddr_in client_address, char *slug)
{
    struct hostent *hostp;
    char *hostaddrp;

    hostp = gethostbyaddr((const char *)&client_address.sin_addr.s_addr, sizeof(client_address.sin_addr.s_addr), AF_INET);
    if (hostp == NULL) error();

    hostaddrp = inet_ntoa(client_address.sin_addr);
    if (hostaddrp == NULL) error();

    display_date();
    printf("Client: %s (%s)\n", hostaddrp, hostp->h_name);

    if (LOG != NULL)
        save_log(slug, hostaddrp, hostp->h_name);
}

void save_log(char *slug, char *hostaddrp, char *h_name)
{
    char contents[256];
    snprintf(contents, sizeof contents, "%s:%s:%s\n", slug, hostaddrp, h_name);

    if (slug != NULL)
        snprintf(contents, sizeof contents, "%s:%s:%s\n", slug, hostaddrp, h_name);
    else
        snprintf(contents, sizeof contents, "%s:%s:%s\n", "error", hostaddrp, h_name);

    FILE *fp;
    fp = fopen(LOG, "a");
    fprintf(fp, "%s", contents);
    fclose(fp);
}

int create_socket()
{
    int lsocket = socket(AF_INET, SOCK_STREAM, 0);
    if (lsocket < 0)
        error();
    else return lsocket;
}

struct sockaddr_in set_address(struct sockaddr_in server_address)
{
    bzero((char *) &server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons((unsigned short)PORT);
    return server_address;
}

void bind_to_port(int listen_socket, struct sockaddr_in server_address)
{
    if (bind(listen_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) 
        error();
    if (listen(listen_socket, QUEUE_SIZE) < 0)
        error();
}

void generate_url(char *buffer, char *slug)
{
    int i;
    memset(slug, '\0', sizeof(slug));

    for (i = 0; i <= SLUG_SIZE - 1; i++)
    {
        int symbol_id = rand_r(&time_seed) % strlen(symbols);
        slug[i] = symbols[symbol_id];
    }

    while (create_directory(slug) == -1)
    {
        int symbol_id = rand_r(&time_seed) % strlen(symbols);
        slug[strlen(slug)] = symbols[symbol_id];
    }

    save_to_file(slug, buffer);
}

int create_directory(char *slug)
{
    char *directory = malloc(strlen(BASEDIR) + strlen(slug));

    strcpy(directory, BASEDIR);
    strcat(directory, slug);

    mkdir(BASEDIR, S_IRWXU | S_IRGRP | S_IROTH | S_IXOTH | S_IXGRP);
    int result = mkdir(directory, S_IRWXU | S_IRGRP | S_IROTH | S_IXOTH | S_IXGRP);

    free(directory);

    return result;
}

void save_to_file(char *slug, char *buffer)
{
    char *directory = malloc(strlen(BASEDIR) + strlen(slug) + strlen("/index.html"));
    strcpy(directory, BASEDIR);
    strcat(directory, slug);
    strcat(directory, "/index.html");

    FILE *fp;
    fp = fopen(directory, "w");
    fprintf(fp, "%s", buffer);
    fclose(fp);

    display_line();

    printf("Saved to: %s\n", directory);
    free(directory);
}

void set_basedir()
{
    BASEDIR = getenv("HOME");
    strcat(BASEDIR, "/code/");
}

void startup_message()
{
    printf("Domain name: %s\n", DOMAIN);
    printf("Saving files to: %s\n", BASEDIR);
    printf("Fiche started listening on port %d.\n", PORT);
}

void parse_parameters(int argc, char **argv)
{
    int c;

    while ((c = getopt (argc, argv, "p:b:q:s:d:o:l:")) != -1)
        switch (c)
        {
            case 'd':
                snprintf(DOMAIN, sizeof DOMAIN, "%s%s%s", "http://", optarg, "/");
                break;
            case 'p':
                PORT = atoi(optarg);
                break;
            case 'b':
                BUFSIZE = atoi(optarg);
                printf("Buffer size set to: %d.\n", BUFSIZE);
                break;
            case 'q':
                QUEUE_SIZE = atoi(optarg);
                printf("Queue size set to: %d.\n", QUEUE_SIZE);
                break;
            case 's':
                SLUG_SIZE = atoi(optarg);
                printf("Slug size set to: %d.\n", SLUG_SIZE);
                break;
            case 'o':
                BASEDIR = optarg;
                if((BASEDIR[strlen(BASEDIR) - 1]) != '/')
                    strcat(BASEDIR, "/");
                break;
            case 'l':
                LOG = optarg;
                printf("Log file: %s\n", LOG);
                break;
            default:
                printf("usage: fiche [-bdpqs].\n");
                printf("                     [-d domain] [-p port] [-s slug_size]\n");
                printf("                     [-o output directory] [-b buffer_size]\n");
                printf("                     [-l log file] [-q queue_size]\n");
                exit(1);
        }
}