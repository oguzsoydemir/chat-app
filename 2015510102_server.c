#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <json/json.h>

#define MAX_CLIENTS 200
#define MAX_ROOMS 50
#define BUFFER_SZ 2048
#define MESSAGE_SIZE 4096
#define NAME_SIZE 100
#define PASS_SIZE 20
#define OTHER 100
#define PORT 3205

/*   Structures   */

typedef enum // Command types
{
    FREE, //   not used -join
    EXIT, //  -exit app
    EXIT_SUCCESSFUL,
    EXIT_FAIL,  //   error -exit app
    EXIT_GROUP, //   exit from group;
    EXIT_GROUP_FAIL,
    EXIT_GROUP_SUCCESS,
    GROUP_CREATE, //  -gcreate
    GROUP_CREATE_FAIL,
    GROUP_CREATE_SUCCESS,
    JOIN_GROUP, //  -join to a group
    JOIN_GROUP_FAIL,
    JOIN_GROUP_SUCCESS,
    JOIN_USERNAME, //  -join to user
    JOIN_USERNAME_FAIL,
    JOIN_USERNAME_SUCCESS,
    MESSAGE,
    MESSAGE_USER,
    MESSAGE_FAIL,  //  -message fail
    SEND_GROUP,    //  -send to group
    SEND_USERNAME, //  -send to user
    USERNAME_FAIL, //   if user exists
    WHOAMI,        //  -whoami
} message_e;

typedef struct // struct for message json
{
    char from[NAME_SIZE];
    char to[NAME_SIZE];
    char message[MESSAGE_SIZE];
} message_t;

typedef struct // struct will send between client and server
{
    message_e type;
    char jObject[MESSAGE_SIZE];
    char other[OTHER];
    char groupName[NAME_SIZE];
    char password[PASS_SIZE];

} package_t;

typedef struct // info about clients
{
    struct sockaddr_in adress;
    int sockfd;
    int uid;
    char name[NAME_SIZE];
} client_t;

typedef struct // about chat rooms
{
    char groupName[NAME_SIZE];
    char password[PASS_SIZE];
    char creatorName[NAME_SIZE];
    client_t *clients[MAX_CLIENTS];
    int gIndex;
} room_t;

/*  Global Variables    */
static _Atomic unsigned int cli_count = 0;
static int uid = 10;
int i = 0;

/*  Functions   */
void print_ip_addr(struct sockaddr_in addr);
void OverwriteStdout();
void Trim(char *arr, int length);
void queue_add(client_t *cl);
void queue_remove(int uid);
void send_message(char *s, int uid);
void *handle_client(void *arg);
void jsonParse(char *jsonString, message_t message);
void SendMessage(package_t p, int sockfd);
void SendToGroup(package_t p);
void SendGroupMessage(package_t p, int sockfd);
void SendPrivate(package_t p);
void JoinUsers(package_t package, client_t *cli);
json_object *convertToJSONObject(message_t message);


client_t *clients[MAX_CLIENTS];
room_t *rooms[MAX_ROOMS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{

    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in server_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // socket ayarları
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
    {
        printf("ERROR: setsockopt\n");
        return EXIT_FAILURE;
    }

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Error: bind\n");
        return EXIT_FAILURE;
    }

    if (listen(listenfd, 10) < 0)
    {
        printf("Error: Listen\n");
        return EXIT_FAILURE;
    }

    printf("\t\tWelcome to DEU Signal v1.0 Chat Program\n");
    printf("\t\t\tServer-Side\n");

    while (1)
    {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);
        if ((cli_count + 1) == MAX_CLIENTS)
        {
            printf("Maximum clients connected. Connection Rejected\n");
            print_ip_addr(cli_addr);
            close(connfd);
            continue;
        }

        /*  Client Settings */
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->adress = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        queue_add(cli);

        pthread_create(&tid, NULL, &handle_client, (void *)cli);

        sleep(1);
    }

    return EXIT_SUCCESS;
}

void print_ip_addr(struct sockaddr_in addr)
{
    printf("%d.%d.%d.%d", addr.sin_addr.s_addr & 0xff, (addr.sin_addr.s_addr & 0xff00) >> 8, (addr.sin_addr.s_addr & 0xff0000) >> 16, (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void OverwriteStdout()
{
    printf("\r%s", ">");
    fflush(stdout);
}

void Trim(char *arr, int length)
{

    for (i = 0; i < length; i++)
    {
        if (arr[i] == '\n')
        {
            arr[i] = '\0';
            break;
        }
    }
}

void queue_add(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid)
{
    pthread_mutex_lock(&clients_mutex);
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *s, int uid)
{
    pthread_mutex_lock(&clients_mutex);
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    printf("Error: write descriptor failed!\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{
    char buffer[BUFFER_SZ];
    char name[32];
    int leave_flag = 0;

    cli_count++;
    client_t *cli = (client_t *)arg;

    //if(recv(cli->sockfd, name, NAME_SIZE, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_SIZE - 1)
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1)
    {
        printf("Enter the name correctly\n");
        leave_flag = 1;
    }
    else
    {
        strcpy(cli->name, name);
        sprintf(buffer, "%s has joined\n", cli->name);
        //printf("%s", buffer);
        //send_message(buffer, cli->uid);
    }
    bzero(buffer, BUFFER_SZ);
    while (1)
    {
        if (leave_flag)
            break;
        package_t package;
        package_t sendPackage;
        int receive = recv(cli->sockfd, &package, sizeof(package_t), 0);
        if (receive > 0)
        {
            int isCreated = 0, isExists = 0, authentication = 0;
            char messageA[BUFFER_SZ];
            switch (package.type)
            {
            case GROUP_CREATE:
                pthread_mutex_lock(&clients_mutex);
                for (i = 0; i < MAX_ROOMS; i++)
                {
                    if (rooms[i])
                    {
                        printf("\n\ngroup:%s pass:%s creator:%s\n", rooms[i]->groupName, rooms[i]->password, rooms[i]->creatorName);

                        if (strcmp(rooms[i]->groupName, package.groupName) == 0)
                        {
                            isExists = 1;
                            break;
                        }
                    }
                    else if (!rooms[i])
                    {

                        room_t *room = (room_t *)malloc(1 * sizeof(room_t));
                        strcpy(room->groupName, package.groupName);
                        strcpy(room->password, package.password);
                        strcpy(room->creatorName, package.other);
                        room->gIndex = 0;
                        printf("group:%s\npass:%s\ncreator:%s\n", room->groupName, room->password, room->creatorName);
                        rooms[i] = room;
                        //printf("group:%s\npass:%s\ncreator:%s\n",rooms[i]->groupName,rooms[i]->password,rooms[i]->creatorName);
                        isCreated = 1;
                        break;
                    }
                }

                if (isCreated == 1)
                {
                    sendPackage.type = GROUP_CREATE_SUCCESS;
                    sprintf(messageA, "Group '%s' has created\n", package.groupName);
                    strcpy(sendPackage.other, messageA);
                }
                else if (isExists == 1)
                {
                    sendPackage.type = GROUP_CREATE_FAIL;
                    sprintf(messageA, "Group '%s' already exists!\n", package.groupName);
                    strcpy(sendPackage.other, messageA);
                    printf("%s-%d\n", messageA, sendPackage.type);
                }
                //SendMessage(sendPackage, cli->sockfd);
                pthread_mutex_unlock(&clients_mutex);
                break;

            case JOIN_GROUP:
                pthread_mutex_lock(&clients_mutex);
                for (i = 0; i < MAX_ROOMS; i++)
                {
                    if (rooms[i])
                    {
                        if (strcmp(rooms[i]->groupName, package.groupName) == 0)
                        {
                            if (strcmp(rooms[i]->password, package.password) == 0)
                            {
                                int k;
                                authentication = 1;
                                for (k = 0; k < MAX_CLIENTS; k++)
                                {
                                    if (!rooms[i]->clients[k])
                                    {
                                        rooms[i]->clients[k] = cli;
                                        isCreated = 1;
                                        break;
                                    }
                                }
                                if (isCreated == 1)
                                {
                                    break;
                                }
                            }
                            else
                            {
                                authentication = 0;
                                break;
                            }
                        }
                    }
                    else if (!rooms[i])
                    {
                        isExists = 1;
                        break;
                    }
                }

                if (isCreated == 1 && authentication == 1)
                {
                    sendPackage.type = JOIN_GROUP_SUCCESS;
                    sprintf(messageA, "%s joined to group '%s'\n", package.other, package.groupName);
                    strcpy(sendPackage.other, messageA);
                    strcpy(sendPackage.groupName, package.groupName);
                    //SendMessage(sendPackage, cli->sockfd);
                    SendToGroup(sendPackage);
                }
                else if (authentication == 0 && isExists == 0)
                {
                    sendPackage.type = JOIN_GROUP_FAIL;
                    sprintf(messageA, "Authentication is not verified for group '%s'\n", package.groupName);
                    strcpy(sendPackage.other, messageA);
                    SendMessage(sendPackage, cli->sockfd);
                }
                else if (isExists == 1)
                {
                    sendPackage.type = JOIN_GROUP_FAIL;
                    sprintf(messageA, "Group '%s' is not exists!\n", package.groupName);
                    strcpy(sendPackage.other, messageA);
                    SendMessage(sendPackage, cli->sockfd);
                }
                pthread_mutex_unlock(&clients_mutex);
                printf("%s\n", messageA);
                break;

            case JOIN_USERNAME:
                pthread_mutex_lock(&clients_mutex);
                JoinUsers(package, cli);
                pthread_mutex_unlock(&clients_mutex);
                break;

            case EXIT:
                pthread_mutex_lock(&clients_mutex);
                printf("-exit index: %d\t other:%s\n", package.type, package.other);
                sendPackage.type = EXIT_SUCCESSFUL;
                sprintf(messageA, "See you later %s\n", cli->name);
                strcpy(sendPackage.other, messageA);
                printf("%s", messageA);
                SendMessage(sendPackage, cli->sockfd);
                leave_flag = 1;
                pthread_mutex_unlock(&clients_mutex);
                break;

            case EXIT_GROUP:
                pthread_mutex_lock(&clients_mutex);
                for (i = 0; i < MAX_ROOMS; i++)
                {
                    if (rooms[i])
                    {
                        if (strcmp(package.other, rooms[i]->groupName) == 0)
                        {
                            int k;
                            for (k = 0; k < MAX_CLIENTS; k++)
                            {
                                if (rooms[i]->clients[k]->sockfd == cli->sockfd)
                                {
                                    rooms[i]->clients[k] = NULL;
                                    isCreated = 1;
                                    break;
                                }
                            }
                            if (isCreated == 1)
                                break;
                        }
                    }
                    if (!rooms[i])
                    {
                        isExists = 1;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                if (isCreated == 1)
                {
                    sendPackage.type = EXIT_GROUP_SUCCESS;
                    strcpy(sendPackage.groupName, package.groupName);
                    sprintf(messageA, "User %s left from '%s'\n", cli->name, package.groupName);
                    strcpy(sendPackage.other, messageA);
                    SendToGroup(sendPackage);
                    SendMessage(sendPackage, cli->sockfd);
                }
                else if (isExists == 1)
                {
                    sendPackage.type = EXIT_GROUP_FAIL;
                    sprintf(messageA, "Group '%s' is not exists", package.groupName);
                    strcpy(sendPackage.other, messageA);
                    SendMessage(sendPackage, cli->sockfd);
                }

                printf("%s\n", messageA);
                pthread_mutex_unlock(&clients_mutex);
                break;
            case SEND_GROUP:
                pthread_mutex_lock(&clients_mutex);
                SendGroupMessage(package, cli->sockfd);
                pthread_mutex_unlock(&clients_mutex);
                break;
            case SEND_USERNAME:
                pthread_mutex_lock(&clients_mutex);
                SendPrivate(package);
                pthread_mutex_unlock(&clients_mutex);
                break;
            case WHOAMI:
                pthread_mutex_lock(&clients_mutex);
                sendPackage.type = WHOAMI;
                sprintf(messageA, "You are '%s'\n", cli->name);
                strcpy(sendPackage.other, messageA);
                printf("%s\n", sendPackage.other);
                SendMessage(sendPackage, cli->sockfd);
                pthread_mutex_unlock(&clients_mutex);
                break;

            default:
                break;
            }
        }
        bzero(buffer, BUFFER_SZ);
        
    }
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());
    return NULL;
}

void SendMessage(package_t p, int sockfd)
{
    if (send(sockfd, &p, sizeof(package_t), 0) < 0)
    {
        perror("Send failed");
        exit(1);
    }
}

void SendToGroup(package_t p)
{

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i])
        {
            if (strcmp(rooms[i]->groupName, p.groupName) == 0)
            {
                int k;
                package_t sendP;
                sendP.type = MESSAGE;
                strcpy(sendP.jObject, p.jObject);
                for (k = 0; k < MAX_CLIENTS; k++)
                {
                    if (rooms[i]->clients[k])
                    {
                        if (send(rooms[i]->clients[k]->sockfd, &p, sizeof(package_t), 0) < 0)
                        {
                            perror("Send failed");
                            exit(1);
                        }
                    }
                    else
                        break;
                }
            }
        }
        else
            break;
    }
}

void SendGroupMessage(package_t p, int sockfd)
{
    message_t message;
    enum json_type type;
    json_object *jobj = json_tokener_parse(p.jObject);
    json_object_object_foreach(jobj, key, val)
    {
        type = json_object_get_type(val);
        switch (type)
        {
        case json_type_string:
            if (strcmp(key, "from") == 0)
                strcpy(message.from, json_object_get_string(val));
            else if (strcmp(key, "to") == 0)
                strcpy(message.to, json_object_get_string(val));
            else if (strcmp(key, "message") == 0)
                strcpy(message.message, json_object_get_string(val));
            break;
        }
    }

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i])
        {
            if (strcmp(rooms[i]->groupName, message.to) == 0)
            {
                int k;
                package_t sendP;
                sendP.type = MESSAGE;
                strcpy(sendP.jObject, p.jObject);
                for (k = 0; k < MAX_CLIENTS; k++)
                {
                    if (rooms[i]->clients[k])
                    {
                        if (rooms[i]->clients[k]->sockfd != sockfd)
                        {
                            SendMessage(sendP, rooms[i]->clients[k]->sockfd);
                        }
                    }
                    else
                        break;
                }
            }
        }
        else
            break;
    }
}

void SendPrivate(package_t p)
{
    message_t message;
    enum json_type type;
    json_object *jobj = json_tokener_parse(p.jObject);
    json_object_object_foreach(jobj, key, val)
    {
        type = json_object_get_type(val);
        switch (type)
        {
        case json_type_string:
            if (strcmp(key, "from") == 0)
                strcpy(message.from, json_object_get_string(val));
            else if (strcmp(key, "to") == 0)
                strcpy(message.to, json_object_get_string(val));
            else if (strcmp(key, "message") == 0)
                strcpy(message.message, json_object_get_string(val));
            break;
        }
    }
    int isExists = 0;
    int toSockfd = 0;

    package_t sendP;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i])
        {
            if (strcmp(clients[i]->name, message.to) == 0)
            {
                isExists = 1;
                toSockfd = clients[i]->sockfd;
                break;
            }
        }
    }
    if (isExists)
    {
        sendP.type = MESSAGE_USER;
        strcpy(sendP.jObject, p.jObject);
        SendMessage(sendP, toSockfd);
    }
    else
    {
        printf("Error: Send private message\n");
    }
}

void JoinUsers(package_t package, client_t *cli)
{
    
    int sockfd_user, isExists = 0;
    message_t message;
    package_t sendPackage;
    char name_user[NAME_SIZE], messageA[BUFFER_SZ];
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i])
        {
            if (strcmp(clients[i]->name, package.groupName) == 0)
            {
                strcpy(sendPackage.groupName, clients[i]->name);
                strcpy(message.to,clients[i]->name);
                strcpy(message.from,cli->name);
                sockfd_user = clients[i]->sockfd;
                strcpy(name_user, clients[i]->name);
                isExists = 1;
                break;
            }
        }
        else if (!clients[i])
        {
            isExists = 0;
            break;
        }
    }
    

    if (isExists == 1)
    {
        sendPackage.type = JOIN_USERNAME_SUCCESS;
        sprintf(messageA, "Private group created by user '%s'\n", cli->name);
        strcpy(message.message,messageA);
        json_object *obj=convertToJSONObject(message);
        strcpy(sendPackage.jObject,json_object_get_string(obj));
        SendMessage(sendPackage, cli->sockfd);
        SendMessage(sendPackage, sockfd_user);
    }
    else
    {
        sendPackage.type = JOIN_USERNAME_FAIL;
        sprintf(messageA, "User '%s' is not exists!\n", package.groupName);
        strcpy(sendPackage.other, messageA);
        SendMessage(sendPackage, cli->sockfd);
    }
    //printf("%s\n", sendPackage.other);
}

json_object *convertToJSONObject(message_t message)
{
    json_object *newObject = json_object_new_object();
    json_object *jFrom = json_object_new_string(message.from);
    json_object *jTo = json_object_new_string(message.to);
    json_object *jMessage = json_object_new_string(message.message);
    json_object_object_add(newObject, "from", jFrom);
    json_object_object_add(newObject, "to", jTo);
    json_object_object_add(newObject, "message", jMessage);
    return newObject;
}
