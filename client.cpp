#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>  

extern int errno;


int port;
int sd; 
pthread_t read_thread; 


void *read_server_responses(void *arg) 
{
    char response[1024];
    while (1) 
    {
        int bytes_read = read(sd, response, sizeof(response) - 1);
        if (bytes_read <= 0) {
            perror("Eroare la citirea raspunsului de la server.\n");
            close(sd);
            exit(errno);
        }
        response[bytes_read] = '\0'; 
        printf("\nRaspuns: %s\n", response);
        printf("Introduceti comanda: "); 
        fflush(stdout); 
    }
    return NULL;
}

int main(int argc, char *argv[]) 
{
    struct sockaddr_in server; 
    char command[100];         
    char name[50];            

   
    if (argc != 3) {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

   
    port = atoi(argv[2]);

    
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket().\n");
        return errno;
    }

   
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) 
    {
        perror("Eroare la connect().\n");
        close(sd);
        return errno;
    }

    
    printf("Introduceti numele soferului (din lista disponibilă): ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';

    
    if (write(sd, name, strlen(name) + 1) <= 0) 
    {
        perror("Eroare la trimiterea numelui catre server.\n");
        close(sd);
        return errno;
    }

    
    if (pthread_create(&read_thread, NULL, read_server_responses, NULL) != 0) 
    {
        perror("Eroare la crearea thread-ului pentru citirea raspunsurilor.\n");
        close(sd);
        return errno;
    }

    
    while (1) 
    {
        
        fflush(stdout); 

        if (fgets(command, sizeof(command), stdin) == NULL) {
            perror("Eroare la citirea comenzii.\n");
            continue;
        }
        command[strcspn(command, "\n")] = '\0';

        
        if (write(sd, command, strlen(command) + 1) <= 0) {
            perror("Eroare la trimiterea comenzii către server.\n");
            close(sd);
            return errno;
        }
    }

    
    close(sd);
    return 0;
}
