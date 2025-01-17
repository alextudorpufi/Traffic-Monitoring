#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sqlite3.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 150

/* codul de eroare returnat de anumite apeluri */
extern int errno;

void initialize_database()
{
    sqlite3 *db;
    char *err_msg = NULL;

    // Creare baza de date
    int rc = sqlite3_open("speed_limits.db", &db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Nu s-a putut deschide baza de date: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    // Creare tabel
    const char *sql = "CREATE TABLE IF NOT EXISTS speed_limits ("
                      "zone INTEGER PRIMARY KEY, "
                      "speed_limit INTEGER); "
                      "INSERT OR IGNORE INTO speed_limits VALUES (1, 50), (2, 90), (3, 140);";

    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Eroare la crearea tabelului: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sqlite3_close(db);
}

void *receive_messages(void *socket_desc)
{
    int sock = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            printf("Conexiunea cu serverul a fost pierduta.\n");
            pthread_exit(NULL); // Inchidem threadul
        }
        printf("Server: %s\n", buffer);
    }

    return NULL;
}

void *send_speed(void *socket_desc)
{
    int sock = *(int *)socket_desc;
    char speed[10]="";

    while (1)
    {   
        sleep(20);
        
        // memset(speed, 0, BUFFER_SIZE);
        // printf("Introduceti viteza: ");
        // fgets(speed, 10, stdin);
        // buffer[strcspn(buffer, "\n")] = '\0';

        //facem o viteza random...
        int random_speed = rand() % 200;
        sprintf(speed, "%d", random_speed);
        // char message[200];
        // snprintf(message, sizeof(message), "Periodic : %s", speed);
        // if (send(sock, message, strlen(message), 0) == -1)
        // {
        //     perror("Failed to send message");
        //     break;
        // }
        if(send(sock, speed, strlen(speed), 0) == -1)
        {
            perror("Failed to send message");
            break;
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int port;
    int sock; // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
    char buffer[BUFFER_SIZE];
    pthread_t recv_thread;
    pthread_t send_thread;

    /* verificare argumente */
    if (argc != 3)
    {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }
    
    /* stabilim portul */
    port = atoi(argv[2]);

    /* cream socketul */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket().\n");
        return errno;
    }

    /* configurare structura server */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    /* conectare la server */
    if (connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("Eroare la connect().\n");
        close(sock);
        return errno;
    }

    initialize_database();

    printf("(Accident : Strada (nume_strada)), (Set-speed : (zona)1/2/3 (viteza)(nr)), (Subscribe/Unsubscribe : Weather/Gas/Sports), (quit) \n");

    // thread pt primit
    if (pthread_create(&recv_thread, NULL, receive_messages, &sock) != 0)
    {
        perror("Eroare la crearea thread-ului de receptie");
        close(sock);
        return -1;
    }

    // thread pt trimis viteza
    if (pthread_create(&send_thread, NULL, send_speed, &sock) != 0)
    {
        perror("Eroare la crearea thread-ului de trimitere");
        close(sock);
        return -1;
    }

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        printf("Poti introduce o comanda! \n");
        fgets(buffer, BUFFER_SIZE, stdin);

        buffer[strcspn(buffer, "\n")] = '\0';

        if (send(sock, buffer, strlen(buffer), 0) == -1)
        {
            perror("Failed to send message");
            break;
        }

        if (strcmp(buffer, "quit") == 0)
        {
            printf("Exiting...\n");
            break;
        }
    }

    close(sock);
    return 0;
}
