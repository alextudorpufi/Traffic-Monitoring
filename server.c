#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>
#include <sqlite3.h>

#define PORT 8080
#define BUFFER_SIZE 150
#define MAX_CLIENTS 100

typedef struct
{ // pentru viteza si zone
    int client_id;
    int speed;
    int zone;
    int valid; // 1 daca clientul a trimis date, 0 altfel
} ClientData;

ClientData client_data[MAX_CLIENTS];
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

int weather_subscribers[MAX_CLIENTS];
int gas_subscribers[MAX_CLIENTS];
int sports_subscribers[MAX_CLIENTS];
int weather_count = 0, gas_count = 0, sports_count = 0;

void send_sports_updates()
{
    while (1)
    {
        sleep(10); // 60

        char message[] = "FCSB a castigat cu 2-1 impotriva celor de la CFR Cluj.";
        for (int i = 0; i < sports_count; i++)
        {
            int client_sock = sports_subscribers[i];
            if (sports_count == 0)
            {
                printf("Nu exista clienti abonati la Sports...\n");
            }
            if (send(client_sock, message, strlen(message), 0) == -1)
            {
                perror("Eroare la trimiterea mesajului catre clientul abonat la Sports");
            }
            else
            {
                printf("Mesaj trimis catre clientul [%d]: %s\n", client_sock - 3, message);
            }
        }
    }
}
void send_weather_updates()
{
    while (1)
    {
        sleep(10); // 60

        char message[] = "Temperatura este de 25 grade Celsius.";
        for (int i = 0; i < weather_count; i++)
        {
            int client_sock = weather_subscribers[i];
            if (weather_count == 0)
            {
                printf("Nu exista clienti abonati la Weather...\n");
            }
            if (send(client_sock, message, strlen(message), 0) == -1)
            {
                perror("Eroare la trimiterea mesajului catre clientul abonat la Weather");
            }
            else
            {
                printf("Mesaj trimis catre clientul [%d]: %s\n", client_sock - 3, message);
            }
        }
    }
}
void send_gas_updates()
{
    while (1)
    {
        sleep(10); // 60

        char message[] = "Pretul benzinei este 7.5 lei/litru.";
        for (int i = 0; i < gas_count; i++)
        {
            int client_sock = gas_subscribers[i];
            if (gas_count == 0)
            {
                printf("Nu exista clienti abonati la Gas...\n");
            }
            if (send(client_sock, message, strlen(message), 0) == -1)
            {
                perror("Eroare la trimiterea mesajului catre clientul abonat la Gas");
            }
            else
            {
                printf("Mesaj trimis catre clientul [%d]: %s\n", client_sock - 3, message);
            }
        }
    }
}

void add_to_subscription(int *subscribers, int *count, int client_sock)
{
    for (int i = 0; i < *count; i++)
    {
        if (subscribers[i] == client_sock)
        {
            send(client_sock, "Client deja abonat!", 20, 0);
            return;
        }
    }
    subscribers[(*count)++] = client_sock;
    send(client_sock, "Abonare reusita!", 17, 0);
}

void remove_from_subscription(int *subscribers, int *count, int client_sock)
{
    for (int i = 0; i < *count; i++)
    {
        if (subscribers[i] == client_sock)
        {
            for (int j = i; j < *count - 1; j++)
            {
                subscribers[j] = subscribers[j + 1];
            }
            (*count)--;
            send(client_sock, "Dezabonare reusita!", 20, 0);
            return;
        }
    }
    send(client_sock, "Clientul nu este abonat!", 25, 0);
}

void set_speed(ClientData *client_data, pthread_mutex_t *data_mutex, int client_sock, char *message)
{
    // // format de tip     Set-speed : 1 100
    // char *token = strtok(message, " "); // stringul ramane : 1 100
    // token = strtok(NULL, " "); // stringul ramane : 1 100
    // int zone = token[0] - '0'; // stringul ramane : 1 100
    // token = strtok(NULL, " "); // stringul ramane : 100
    // int speed = atoi(token); // stringul ramane : 100

    // format de tip    Set-speed : 1 100
    int zone = message[12] - '0';
    // transmitem zona corespunzatoare si viteza din tabela sqlite
    int speed = atoi(message + 14);

    pthread_mutex_lock(data_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_data[i].client_id == client_sock)
        {
            client_data[i].speed = speed;
            client_data[i].zone = zone;
            client_data[i].valid = 1;
            break;
        }
        if (client_data[i].client_id == 0)
        {
            client_data[i].client_id = client_sock;
            client_data[i].speed = speed;
            client_data[i].zone = zone;
            client_data[i].valid = 1;
            break;
        }
    }
    pthread_mutex_unlock(data_mutex);
    send(client_sock, "Viteza setata cu succes!", 24, 0);
}

void remove_speed_data(ClientData *client_data, pthread_mutex_t *data_mutex, int client_sock)
{
    pthread_mutex_lock(data_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_data[i].client_id == client_sock)
        {
            client_data[i].client_id = 0;
            client_data[i].speed = 0;
            client_data[i].zone = 0;
            client_data[i].valid = 0;
            break;
        }
    }
    pthread_mutex_unlock(data_mutex);
}
void send_speed_notifications(ClientData *client_data, pthread_mutex_t *data_mutex)
{
    while (1)
    {
        sleep(20); // vedem

        pthread_mutex_lock(data_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_data[i].valid == 1)
            {
                char message[200];
                // cautam viteza maxima in baza de date pe baza zonei:

                sqlite3 *db;
                char *err_msg = NULL;
                int rc = sqlite3_open("speed_limits.db", &db);
                if (rc != SQLITE_OK)
                {
                    fprintf(stderr, "Nu s-a putut deschide baza de date: %s\n", sqlite3_errmsg(db));
                    exit(EXIT_FAILURE);
                }
                // cautam viteza corespunzatoare zonei:
                char sql[100];
                snprintf(sql, sizeof(sql), "SELECT speed_limit FROM speed_limits WHERE zone = %d;", client_data[i].zone);
                sqlite3_stmt *stmt;

                rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
                if (rc != SQLITE_OK)
                {
                    fprintf(stderr, "Eroare la pregătirea interogării: %s\n", sqlite3_errmsg(db));
                    sqlite3_close(db);
                    pthread_mutex_unlock(data_mutex);
                    exit(EXIT_FAILURE);
                }

                int speed_limit;
                rc = sqlite3_step(stmt);
                if (rc == SQLITE_ROW)
                {
                    speed_limit = sqlite3_column_int(stmt, 0);
                }
                else
                {
                    fprintf(stderr, "Zona %d nu a fost găsită în baza de date.\n", client_data[i].zone);
                }

                // Finalizează interogarea
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                // verificam daca viteza clientului este mai mare decat viteza maxima
                if (client_data[i].speed >= speed_limit)
                {
                    snprintf(message, sizeof(message), "Viteza maxima in zona %d este de %d km/h. Ati depasit viteza cu %d km/h.",
                             client_data[i].zone, speed_limit, client_data[i].speed - speed_limit);
                    if (send(client_data[i].client_id, message, strlen(message), 0) == -1)
                    {
                        perror("Eroare la trimiterea mesajului catre clientul abonat la Speed");
                    }
                    else
                    {
                        printf("Mesaj trimis catre clientul [%d]: %s\n", client_data[i].client_id - 3, message);
                    }
                }
                else
                {
                    snprintf(message, sizeof(message), "Viteza maxima in zona %d este de %d km/h. Felicitari pentru respectarea vitezei maxime!",
                             client_data[i].zone, speed_limit);
                    if (send(client_data[i].client_id, message, strlen(message), 0) == -1)
                    {
                        perror("Eroare la trimiterea mesajului catre clientul abonat la Speed");
                    }
                    else
                    {
                        printf("Mesaj trimis catre clientul [%d]: %s\n", client_data[i].client_id - 3, message);
                    }
                }

                // sprintf(message, "Viteza maxima in zona %d este de %d km/h.", client_data[i].zone, client_data[i].speed);
                // if (send(client_data[i].client_id, message, strlen(message), 0) == -1)
                // {
                //     perror("Eroare la trimiterea mesajului catre clientul abonat la Speed");
                // }
                // else
                // {
                //     printf("Mesaj trimis catre clientul [%d]: %s\n", client_data[i].client_id - 3, message);
                // }
            }
        }
        pthread_mutex_unlock(data_mutex);
    }
}

// void send_accident(ClientData *client_data, pthread_mutex_t *data_mutex, int client_sock, char *message)
// {
//     char accident[100];
//     snprintf(accident, sizeof(accident), "Accident pe strada %s!", message + 10);

//     int subscribers[MAX_CLIENTS];
//     int count = 0;

//     pthread_mutex_lock(data_mutex);
//     for (int i = 0; i < MAX_CLIENTS; i++)
//     {
//         if (client_data[i].valid == 1)
//         {
//             subscribers[count++] = client_data[i].client_id;
//         }
//     }
//     pthread_mutex_unlock(data_mutex);

//     for (int i = 0; i < count; i++)
//     {
//         if (send(subscribers[i], accident, strlen(accident), 0) == -1)
//         {
//             perror("Eroare la trimiterea mesajului catre clientul abonat la Speed");
//         }
//         else
//         {
//             printf("Mesaj trimis catre clientul [%d]: %s\n", subscribers[i] - 3, accident);
//         }
//     }
//     send(client_sock, "Mesaj trimis tuturor clientilor!", 30, 0);
// }

void send_accident(ClientData *client_data, pthread_mutex_t *data_mutex, int *client_sockets, int client_sock, char *message)
{
    // extragem mesajul
    char broadcast_message[200];
    strcpy(broadcast_message, "Accident in zona:");
    strcat(broadcast_message, message + 10);

    printf("Anuntam mesajul de la [%d]: %s\n", client_sock - 3, broadcast_message);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sockets[i] != 0)
        {
            if (client_sockets[i] != client_sock)
            {
                if (send(client_sockets[i], broadcast_message, strlen(broadcast_message), 0) == -1)
                {
                    perror("Eroare la trimiterea mesajului de broadcast");
                }
                else
                {
                    printf("Mesajul a fost trimis catre clientul [%d]: %s\n", client_sockets[i] - 3, broadcast_message);
                }
            }
            else // pt cel ce a trimis mesajul
            {
                send(client_sock, "Mesajul a fost trimis cu succes celorlalti clienti!", 50, 0);
            }
        }
    }
}

void *print_client_speeds(void *arg)
{
    ClientData *client_data = (ClientData *)arg;

    while (1)
    {
        sleep(20); // 60
        printf("Vitezele clientilor:\n");
        pthread_mutex_lock(&data_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_data[i].valid == 1)
            {
                printf("Client [%d] : %d km/h\n", client_data[i].client_id - 3, client_data[i].speed);
            }
        }
        pthread_mutex_unlock(&data_mutex);
        printf("------------------------------\n");
    }
}

void process_message(char *message, int client_sock, int *client_sockets)
{
    // verificam daca e un nr intre 0 si 200
    //  if(atoi(message) >= 0 && atoi(message) <= 200)
    //  {
    //      int speed = atoi(message);
    //      printf("Viteza primita de la clientul [%d] : %s\n", client_sock - 3, message);
    //      return;
    //  }
    if (strncmp(message, "Subscribe :", 11) == 0)
    {
        if (strstr(message, "Weather"))
        {
            add_to_subscription(weather_subscribers, &weather_count, client_sock);
        }
        else if (strstr(message, "Gas"))
        {
            add_to_subscription(gas_subscribers, &gas_count, client_sock);
        }
        else if (strstr(message, "Sports"))
        {
            add_to_subscription(sports_subscribers, &sports_count, client_sock);
        }
        else
        {
            send(client_sock, "Subscriptie necunoscuta!", 25, 0);
        }
    }
    else if (strncmp(message, "Unsubscribe :", 13) == 0)
    {
        if (strstr(message, "Weather"))
        {
            remove_from_subscription(weather_subscribers, &weather_count, client_sock);
        }
        else if (strstr(message, "Gas"))
        {
            remove_from_subscription(gas_subscribers, &gas_count, client_sock);
        }
        else if (strstr(message, "Sports"))
        {
            remove_from_subscription(sports_subscribers, &sports_count, client_sock);
        }
        else
        {
            send(client_sock, "Subscriptie necunoscuta!", 25, 0);
        }
    }
    else if (strncmp(message, "Set-speed :", 11) == 0)
    {
        if (message[12] != '1' && message[12] != '2' && message[12] != '3')
        {
            send(client_sock, "Zona invalida!", 17, 0);
            return;
        }

        set_speed(client_data, &data_mutex, client_sock, message);
    }
    // else if (strncmp(message, "Accident :", 9) == 0)
    // {
    //     char accident[100];
    //     snprintf(accident, sizeof(accident), "Accident pe strada %s!", message + 10);
    //     printf("%s\n", accident);
    //     printf("Mesajul se trimite tuturor celorlalti clienti!\n");
    //     send_accident(client_data, &data_mutex,client_sock, message);
    // }
    else if (strncmp(message, "Accident :", 10) == 0)
    {
        send_accident(client_data, &data_mutex, client_sockets, client_sock, message);
    }
    else if (strcmp(message, "quit") == 0)
    {
        // dezabonam clientul de la toate
        remove_from_subscription(weather_subscribers, &weather_count, client_sock);
        remove_from_subscription(gas_subscribers, &gas_count, client_sock);
        remove_from_subscription(sports_subscribers, &sports_count, client_sock);
        remove_speed_data(client_data, &data_mutex, client_sock);
        send(client_sock, "quit", 5, 0);
    }
    else if (strncmp(message, "Periodic :", 10) == 0)
    {
        printf("Viteza client [%d] : %s\n", client_sock - 3, message + 10);
    }
    else if (atoi(message) >= 0 && atoi(message) <= 200)
    {
        // am primit viteza
        send(client_sock, "Viteza primita cu succes!", 26, 0);
    }
    else
    {
        send(client_sock, "Comanda necunoscuta!", 22, 0);
    }
}

int main()
{
    int server_sock, new_sock, max_sd, activity, valread, sd;
    int client_sockets[MAX_CLIENTS] = {0};
    struct sockaddr_in address;
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 3) < 0)
    {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    // Gas Thread
    pthread_t gas_update_thread;
    if (pthread_create(&gas_update_thread, NULL, (void *)send_gas_updates, NULL) != 0)
    {
        perror("Eroare la crearea thread-ului pentru actualizari Gas");
        exit(EXIT_FAILURE);
    }

    // Weather Thread
    pthread_t weather_update_thread;
    if (pthread_create(&weather_update_thread, NULL, (void *)send_weather_updates, NULL) != 0)
    {
        perror("Eroare la crearea thread-ului pentru actualizari Weather");
        exit(EXIT_FAILURE);
    }

    // Sports Thread
    pthread_t sports_update_thread;
    if (pthread_create(&sports_update_thread, NULL, (void *)send_sports_updates, NULL) != 0)
    {
        perror("Eroare la crearea thread-ului pentru actualizari Sports");
        exit(EXIT_FAILURE);
    }

    // Speed Thread
    pthread_t speed_update_thread;
    if (pthread_create(&speed_update_thread, NULL, (void *)send_speed_notifications, &client_data) != 0)
    {
        perror("Eroare la crearea thread-ului pentru actualizari Speed");
        exit(EXIT_FAILURE);
    }

    // Print Speeds Thread
    pthread_t print_speeds_thread;
    if (pthread_create(&print_speeds_thread, NULL, (void *)print_client_speeds, &client_data) != 0)
    {
        perror("Eroare la crearea thread-ului pentru afisarea vitezelor");
        exit(EXIT_FAILURE);
    }

    printf("Serverul este in asteptare...\n");

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(server_sock, &readfds);
        max_sd = server_sock;

        // Adaugam socketii clientilor in multimea de descriptori de citire
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_sockets[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // asteptam activitate pe socketzi
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR))
        {
            perror("Select error");
        }

        if (FD_ISSET(server_sock, &readfds))
        {
            // acceptam conexiuni
            int addrlen = sizeof(address);
            if ((new_sock = accept(server_sock, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("Accept");
                exit(EXIT_FAILURE);
            }
            printf("Client [%d] conectat: socket %d\n", new_sock - 3, new_sock);

            // adaugam noul socket in lista de socketi
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_sockets[i] == 0)
                {
                    client_sockets[i] = new_sock;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_sockets[i];

            if (FD_ISSET(sd, &readfds))
            {
                memset(buffer, 0, BUFFER_SIZE);
                valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0)
                {
                    close(sd);
                    client_sockets[i] = 0;
                }
                else
                {

                    buffer[valread] = '\0';
                    // if (atoi(buffer) >= 0 && atoi(buffer) <= 200)
                    // {
                    //     int speed = atoi(buffer);
                    //     printf("Viteza primita de la clientul [%d] : %s\n", sd - 3, buffer);
                    //     continue;
                    // }

                    printf("Comanda primita de la clientul [%d] : %s\n", sd - 3, buffer);
                    process_message(buffer, sd, client_sockets);
                }
            }
        }
    }

    return 0;
}