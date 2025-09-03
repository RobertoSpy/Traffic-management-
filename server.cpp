#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <mysql/mysql.h>
#include <arpa/inet.h>



#define PORT 2909


extern int errno;

static void *treat(void *); 
void comenzi(int client, MYSQL *conn, const char *command, const char *name);
void baza_procesare(MYSQL *conn, const char *query, char *response);
void nume_client(int client, MYSQL* conn); 


typedef struct {
    pthread_t idThread; 
    int thCount;        
} Thread;

Thread *threadsPool; 

typedef struct {
    int vreme;   
    int evenimente;    
    int preturi; 
    int active;   
} ExtraOptions;

ExtraOptions clientOptions[1000]; 


int sd;             
int nthreads;   
pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER; 

MYSQL *global_conn; 

MYSQL *init_mysql_connection() 
{
    MYSQL *conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, "localhost", "root", "parola_noua", "baza_date", 0, NULL, 0)) {
        fprintf(stderr, "Eroare la conectarea la baza de date MySQL: %s\n", mysql_error(conn));
        exit(EXIT_FAILURE);
    }
    return conn;
}


void close_mysql_connection(MYSQL *conn) 
{
    mysql_close(conn);
}

void golire(int signum)
 {
    printf("\n Curatare resurse...\n");

    
    char query[256];
    snprintf(query, sizeof(query), "UPDATE Soferi SET activ = 0");
    if (mysql_query(global_conn, query)) {
        perror("Eroare la actualizarea starii deconectate în baza de date\n");
    }

    snprintf(query, sizeof(query), "UPDATE Optiuni SET vreme = 0, evenimente_sportive = 0, preturi_peco = 0");
    if (mysql_query(global_conn, query)) 
    {
        perror("Eroare la resetarea optiunilor în baza de date\n");
    }

       snprintf(query, sizeof(query), "UPDATE Soferi SET socket_descriptor = NULL");
    if (mysql_query(global_conn, query)) {
        perror("Eroare la actualizarea starii deconectate în baza de date\n");
    }

   
    if (global_conn != NULL) {
        mysql_close(global_conn);
    }

    printf("Server oprit\n");
    exit(0);
}


void *frecventa_viteza(void *arg) {
    while (1) {
         
         sleep(60);

        char atentie[256];
        char query_limit[256];
        MYSQL *conn = init_mysql_connection();

        
        const char *query = "SELECT socket_descriptor, viteza, strada FROM Soferi WHERE activ = 1";
        if (mysql_query(conn, query)) {
            fprintf(stderr, "Eroare la interogarea clientilor activi: %s\n", mysql_error(conn));
            continue;
        }

        MYSQL_RES *result = mysql_store_result(conn);
        if (result && mysql_num_rows(result) > 0) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                int socket_descriptor = atoi(row[0]);  
                int viteza_client = atoi(row[1]);      
                char *strada_client = row[2];          

                
                
                     snprintf(query_limit, sizeof(query_limit), "SELECT viteza_restrictie FROM Strazi WHERE nume='%s'", strada_client);
                if (mysql_query(conn, query_limit)) {
                    fprintf(stderr, "Eroare la interogarea limitei de viteza pentru strada %s: %s\n", strada_client, mysql_error(conn));
                    continue;
                }

                MYSQL_RES *result_limita = mysql_store_result(conn);
                if (result_limita && mysql_num_rows(result_limita) > 0) {
                    MYSQL_ROW row_limita = mysql_fetch_row(result_limita);
                    int viteza_limita = atoi(row_limita[0]);  
                    mysql_free_result(result_limita);

                   
                    
          if (viteza_client > viteza_limita) 
          {
                        snprintf(atentie, sizeof(atentie), "Ai depasit limita de viteza pe strada %s! Limita este %d km/h.\n", strada_client, viteza_limita);
                    } else {
                        snprintf(atentie, sizeof(atentie), "Viteza ta este în limita legala pe strada %s. Limita este %d km/h.\n", strada_client, viteza_limita);
                    }

                    
                    printf("Clientul cu socket_descriptor %d %s\n", socket_descriptor, atentie);

                    
                    if (write(socket_descriptor, atentie, strlen(atentie) + 1) <= 0) {
                        perror("Eroare la trimiterea mesajului către client.");
                    }
                } else 
                {
                    fprintf(stderr, "Limita de viteza pentru strada %s nu a fost găsita\n", strada_client);
                }
            }
        } 
        else 
        {
            fprintf(stderr, "Nu sunt clienti activi în tabela soferi.\n");
        }

        mysql_free_result(result);
        close_mysql_connection(conn);
    }

    return NULL;
}


int main(int argc, char *argv[]) {
    struct sockaddr_in server;
    global_conn = init_mysql_connection();
    if (argc < 2) {
        fprintf(stderr, "Eroare: Primul argument este numarul de fire de executie...\n");
        exit(1);
    }
    nthreads = atoi(argv[1]);
    if (nthreads <= 0) {
        fprintf(stderr, "Eroare: Numar de fire invalid...\n");
        exit(1);
    }
    threadsPool = (Thread*)calloc(nthreads, sizeof(Thread));

 
    signal(SIGINT, golire);
    signal(SIGTERM, golire);

    
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket().\n");
        return errno;
    }

    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
        perror("Eroare la bind()\n");
        return errno;
    }

    if (listen(sd, 5) == -1) {
        perror("Eroare la listen().\n");
        return errno;
    }

     
    pthread_t speed_thread;
    if (pthread_create(&speed_thread, NULL, frecventa_viteza, NULL) != 0) {
        perror("Eroare la crearea thread-ului pentru viteza");
        exit(EXIT_FAILURE);
    }

    printf("Nr threaduri %d \n", nthreads);
    fflush(stdout);

    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threadsPool[i].idThread, NULL, &treat, (void *)(long)i);
    }

    for (;;) {
        printf("Asteptam la portul %d...\n", PORT);
        pause();
    }
}

void *treat(void *arg) {
    int client;
    struct sockaddr_in from;
    bzero(&from, sizeof(from));
    printf("[thread] - %ld - pornit...\n", (long)arg);
    fflush(stdout);

    MYSQL *conn = init_mysql_connection();

    for (;;) {
        socklen_t length = sizeof(from); 
        pthread_mutex_lock(&mlock);

        if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0) {
            perror(" Eroare la accept().");
            pthread_mutex_unlock(&mlock); 
            continue;
        }

        pthread_mutex_unlock(&mlock);

        threadsPool[(long)arg].thCount++;

        
        nume_client(client, conn);

        close(client);
    }

    close_mysql_connection(conn);
    return NULL;
}

pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;



bool nume_validat(MYSQL* conn, const char* name) {
    char query[256];
    sprintf(query, "SELECT COUNT(*) FROM Soferi WHERE nume='%s'", name);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "Eroare MySQL: %s\n", mysql_error(conn));
        return false;
    }
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        fprintf(stderr, "Eroare MySQL: %s\n", mysql_error(conn));
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    bool exists = (atoi(row[0]) > 0); 
    mysql_free_result(result);
    return exists;
}



void nume_client(int client, MYSQL* conn) {
    char name[50], primire[1024];
    char query[256];
   
    
    while (1) {
        if (read(client, name, sizeof(name)) <= 0) 
        {
            perror(" Eroare la citirea numelui.\n");
            close(client);
            return;
        }

        if (nume_validat(conn, name)) 
        {
            snprintf(primire, sizeof(primire), "Bun venit, %s! Acces permis.\n", name);
            write(client, primire, strlen(primire) + 1);

            
            
            snprintf(query, sizeof(query), "UPDATE Soferi SET activ = 1 WHERE nume = '%s'", name);
            if (mysql_query(conn, query)) {
                fprintf(stderr, " Eroare la actualizarea stării 'activ' în baza de date: %s\n", mysql_error(conn));
            } else {
                printf(" Clientul %s a fost setat ca activ.\n", name);
            }

            
            snprintf(query, sizeof(query), "UPDATE Soferi SET socket_descriptor = %d WHERE nume = '%s'", client, name);
            if (mysql_query(conn, query)) {
                fprintf(stderr, " Eroare la salvarea socket_descriptor în baza de date: %s\n", mysql_error(conn));
            } else {
                printf("socket_descriptor salvat pentru clientul %s.\n", name);
            }

            break; 
        } else {
            snprintf(primire, sizeof(primire), "Nume invalid. Va rugam să incercati din nou.\n");
            write(client, primire, strlen(primire) + 1);
        }
    }

    
    while (1)
     {
        char command[100];
        int bytes_read = read(client, command, sizeof(command));
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf(" Clientul %s s a deconectat.\n", name);
            } else {
                perror("Eroare la citirea comenzii.\n");
            }
            break;
        }
        sleep(1);
        comenzi(client, conn, command, name);
    }

    
    
    snprintf(query, sizeof(query), "UPDATE Optiuni SET vreme = 0, evenimente_sportive = 0, preturi_peco = 0 WHERE sofer_id = (SELECT id FROM Soferi WHERE nume='%s')", name);
    if (mysql_query(conn, query)) {
        fprintf(stderr, " Eroare la resetarea optiunilor pentru client: %s\n", mysql_error(conn));
    } 
    else 
    {
        printf("Optiunile au fost resetate pentru clientul %s.\n", name);
    }

    
    snprintf(query, sizeof(query), "UPDATE Soferi SET activ = 0 WHERE nume = '%s'", name);
    if (mysql_query(conn, query)) {
        fprintf(stderr, " Eroare la actualizarea starii 'activ' pentru client: %s\n", mysql_error(conn));
    } 
    else 
    {
        printf("Starea 'activ' a fost resetata pentru clientul %s.\n", name);
    }

    
    snprintf(query, sizeof(query), "UPDATE Soferi SET socket_descriptor = NULL WHERE nume = '%s'", name);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "Eroare la resetarea socket_descriptor în baza de date: %s\n", mysql_error(conn));
    } 
    else 
    {
        printf(" socket_descriptor resetat pentru clientul %s.\n", name);
    }

    
    close(client);
}



void comenzi(int client, MYSQL *conn, const char *command, const char *name) {
    char query[1024];
    char primire[4096] = "";
    char strada_noua[100];

    
    
    
    int driver_id = 0;
    sprintf(query, "SELECT id FROM Soferi WHERE nume='%s'", name);
    MYSQL_RES *result = NULL;
    if (mysql_query(conn, query)) {
        strcpy(primire, "Eroare la interogarea bazei de date.\n");
        write(client, primire, strlen(primire) + 1);
        return;
    }
    result = mysql_store_result(conn);
    if (result && mysql_num_rows(result) > 0) {
        MYSQL_ROW row = mysql_fetch_row(result);
        driver_id = atoi(row[0]);
    }
    mysql_free_result(result);

    
    if (driver_id == 0) {
        strcpy(primire, "Eroare: Numele șoferului nu este valid.\n");
        write(client, primire, strlen(primire) + 1);
        return;
    }

if (strncmp(command, "schimb pe strada:", 17) == 0) {
    
    sscanf(command + 17, "%s", strada_noua);

    sprintf(query, "SELECT COUNT(*) FROM Strazi WHERE nume='%s'", strada_noua);
    if (mysql_query(conn, query)) {
        perror("Eroare la interogare MySQL.");
        return;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    bool strada = (atoi(row[0]) > 0);
    mysql_free_result(result);

    if (!strada) 
    {
        strcpy(primire, "Strada nu exista în baza de date.\n");
    } else {
        sprintf(query, "UPDATE Soferi SET strada='%s' WHERE nume='%s'", strada_noua, name);
        baza_procesare(conn, query, primire);

        sprintf(query, "SELECT viteza_restrictie FROM Strazi WHERE nume='%s'", strada_noua);
        if (mysql_query(conn, query)) {
            perror("Eroare la interogare MySQL pentru limită de viteza.");
            return;
        }
        result = mysql_store_result(conn);
        row = mysql_fetch_row(result);
        int speed_limit = atoi(row[0]);
        mysql_free_result(result);

        sprintf(primire, "Strada a fost schimbata cu succes. Limita de viteza este: %d km/h.\n", speed_limit);
    }

    write(client, primire, strlen(primire) + 1);
}
    
    if (strncmp(command, "bifez optiuni extra:", 20) == 0) 
    {
       
        if (strstr(command, "vreme")) {
            sprintf(query, "UPDATE Optiuni SET vreme=1 WHERE sofer_id=%d", driver_id);
            baza_procesare(conn, query, primire);
             clientOptions[client].vreme = 1;
        }
        if (strstr(command, "evenimente")) {
            sprintf(query, "UPDATE Optiuni SET evenimente_sportive=1 WHERE sofer_id=%d", driver_id);
            baza_procesare(conn, query, primire);
             clientOptions[client].evenimente = 1;
        }
        if (strstr(command, "preturi")) {
            sprintf(query, "UPDATE Optiuni SET preturi_peco=1 WHERE sofer_id=%d", driver_id);
            baza_procesare(conn, query, primire);
            clientOptions[client].preturi = 1;
        }
        strcpy(primire, "Opțiunile extra au fost actualizate.\n");
    } 
    else if (strncmp(command, "vrea informatii despre", 22) == 0) {
        
        if (strstr(command, "vreme") && clientOptions[client].vreme==1) {
            sprintf(query, "SELECT vreme FROM Strazi WHERE nume = (SELECT strada FROM Soferi WHERE id=%d)", driver_id);
            baza_procesare(conn, query, primire);
        }
        else if (strstr(command, "evenimente") && clientOptions[client].evenimente==1) {
            sprintf(query, "SELECT evenimente_sportive FROM Strazi WHERE nume = (SELECT strada FROM Soferi WHERE id=%d)", driver_id);
            baza_procesare(conn, query, primire);
        }
        else if (strstr(command, "preturi") && clientOptions[client].preturi==1) {
            sprintf(query, "SELECT motorina_peco, benzina_peco FROM Strazi WHERE nume = (SELECT strada FROM Soferi WHERE id=%d)", driver_id);
            baza_procesare(conn, query, primire);
        }
    } 
    
     else if (strncmp(command, "raporteaza accident:", 20) == 0) {
    char accident_street[100];
    char notification[256];
    sscanf(command + 20, "%s", accident_street);

    
    sprintf(query, "SELECT COUNT(*) FROM Strazi WHERE nume='%s'", accident_street);
    if (mysql_query(conn, query)) 
    {
        perror(" Eroare la verificarea străzii în baza de date");
        return;
    }
    MYSQL_RES* result = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    bool strada = (atoi(row[0]) > 0);
    mysql_free_result(result);

    if (!strada) 
    {
        strcpy(primire, "Strada nu exista în baza de date.\n");
    } else {
        
        sprintf(query, "UPDATE Strazi SET viteza_restrictie = 30 WHERE nume='%s'", accident_street);
        if (mysql_query(conn, query)) {
            perror(" Eroare la actualizarea vitezei în baza de date.");
            return;
        }

       
        sprintf(query, "SELECT socket_descriptor FROM Soferi WHERE activ = 1 AND socket_descriptor IS NOT NULL");
        if (mysql_query(conn, query)) {
            perror("Eroare la interogarea șoferilor activi.");
            return;
        }
        result = mysql_store_result(conn);
        if (result) 
        {
            MYSQL_ROW active_row;
            while ((active_row = mysql_fetch_row(result))) {
                int active_socket = atoi(active_row[0]);
                if (active_socket <= 0) {
                    printf("Socket descriptor invalid: %d\n", active_socket);
                    continue;
                }
                printf(" Socket descriptor activ: %d\n", active_socket);
                
                
                
                snprintf(notification, sizeof(notification), 
                         "Atentie: accident raportat pe strada %s. Limita de viteza este acum 30 km/h.\n", 
                         accident_street);

                
                if (write(active_socket, notification, strlen(notification) + 1) <= 0) {
                    perror("Eroare la trimiterea notificării către un client activ.");
                } else 
                {
                    printf(" Mesaj trimis clientului cu socketul %d.\n", active_socket);
                }
            }
            mysql_free_result(result);
        }

        strcpy(primire, "Accident raportat cu succes. Toti soferii activi au fost notificati.\n");
    }

    write(client, primire, strlen(primire) + 1);
    
}


 else if (strncmp(command, "update viteza", 13) == 0) 
 {
    int new_speed;
    sscanf(command + 14, "%d", &new_speed);

    
    sprintf(query, "UPDATE Soferi SET viteza=%d WHERE nume='%s'", new_speed, name);
    baza_procesare(conn, query, primire);

    sprintf(query, "SELECT viteza_restrictie FROM Strazi WHERE nume=(SELECT strada FROM Soferi WHERE nume='%s')", name);
    baza_procesare(conn, query, primire);

    int speed_limit = atoi(primire);
    if (new_speed > speed_limit) {
        strcat(primire, "Ai depasit limita de viteza\n");
    } else {
        strcat(primire, "Viteza este în limita legala.\n");
    }

    write(client, primire, strlen(primire) + 1);

}    else if (strncmp(command, "debifare optiuni", 16) == 0) 
{
        
        sprintf(query, "UPDATE Optiuni SET vreme=0, evenimente_sportive=0, preturi_peco=0 WHERE sofer_id=%d", driver_id);
        if (mysql_query(conn, query)) {
            strcpy(primire, "Eroare la resetarea optiunilor.\n");
        } else {
            strcpy(primire, "Optiunile au fost resetate.\n");
        }

        
        write(client, primire, strlen(primire) + 1);
        clientOptions[client].vreme= 0;
        clientOptions[client].evenimente = 0;
        clientOptions[client].preturi = 0;
        

        
    }else 
    {
        strcpy(primire, "Comanda necunoscuta\n");
    }

    
    if (write(client, primire, strlen(primire) + 1) <= 0) {
        perror("Eroare la write() catre client.");
        close(client);
        return;
    }
}

void baza_procesare(MYSQL *conn, const char *query, char *response) {
    
    memset(response, 0, 4096);
    unsigned int response_len = 0;

    pthread_mutex_lock(&db_lock); 
    if (mysql_query(conn, query)) {
        snprintf(response, 4096, "Eroare la executarea interogarii: %s\n", mysql_error(conn));
        pthread_mutex_unlock(&db_lock); 
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    pthread_mutex_unlock(&db_lock); 

    if (!result) 
    {
        snprintf(response, 4096, "Eroare la preluarea rezultatelor: %s\n", mysql_error(conn));
        return;
    }

    
    MYSQL_ROW row;
    unsigned int num_fields = mysql_num_fields(result);
    

    while ((row = mysql_fetch_row(result))) 
    {
        for (unsigned int i = 0; i < num_fields; i++) 
        {
            if (response_len + 1024 >= 4096) 
            { 
                snprintf(response + response_len, 4096 - response_len, "Rezultatul este prea mare pentru a fi afișat complet.\n");
                mysql_free_result(result);
                return;
            }
            response_len += snprintf(response + response_len, 4096 - response_len, "%s\t", row[i] ? row[i] : "NULL");
        }
        response_len += snprintf(response + response_len, 4096 - response_len, "\n");
    }

    if (response_len == 0) 
    {
        snprintf(response, 4096, "Interogarea nu a returnat niciun rezultat.\n");
    }

    mysql_free_result(result);
}