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

/*  Defines */
#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define MESSAGE_SIZE 4096
#define NAME_SIZE 100
#define PASS_SIZE 20
#define PHONE_SIZE 10
#define OTHER 100
#define PORT 3205

#define WARNING "\x1B[31m"  //RED
#define ANNOUNCE "\x1B[32m" //GREEN
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define PTND "\x1B[35m"  //MAG
#define TITLE "\x1B[36m" //CYN
#define RESET "\x1B[0m"

/*  Structs */

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

/*  Global Variables    */
volatile sig_atomic_t flag = 0;
int sockfd = 0, listenfd = 0;
int i = 0;
char name[NAME_SIZE];
int debug = 1;
message_e status = FREE;
message_t clientInfo;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/*  Functions   */
void OverwriteStdout();
void Trim(char *arr, int length);
void catch_crtl_c_and_exit();
void ReceiveHandler();
void SendHandler();
void Message(package_t p);
json_object *convertToJSONObject(message_t message);
void jsonParse(json_object *jobj, message_t message);
void PrintScreen();
void MessagePrivate(package_t p);

int main(int argc, char **argv)
{
    signal(SIGINT, catch_crtl_c_and_exit);
    printf("Enter your username or phone number : ");
    fgets(name, NAME_SIZE, stdin);
    Trim(name, strlen(name));
    strcpy(clientInfo.from, name);
    if (strlen(clientInfo.from) > NAME_SIZE - 1)
    {
        printf("Enter name correctly\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1)
    {
        printf("Error: Connect\n");
        return EXIT_FAILURE;
    }

    send(sockfd, clientInfo.from, NAME_SIZE, 0);
    printf("\t\tWelcome to DEU Signal v1.0 Chat Program\n");
    PrintScreen();
    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *)SendHandler, NULL) != 0)
    {
        printf("Error for send pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void *)ReceiveHandler, NULL) != 0)
    {
        printf("Error for receive pthread\n");
        return EXIT_FAILURE;
    }

    while (1)
    {
        if (flag)
        {
            
            break;
        }
    }
    close(sockfd);

    return EXIT_SUCCESS;
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

void catch_crtl_c_and_exit()
{
    flag = 1;
}

void ReceiveHandler()
{
    int isExit = 0;
    while (1)
    {

        if (isExit == 1)
        {
            break;
        }
        OverwriteStdout();
        package_t package;
        message_t message;
        int receive = recv(sockfd, &package, sizeof(package_t), 0);
        if (receive > 0)
        {
            pthread_mutex_lock(&clients_mutex);

            switch (package.type)
            {
            case GROUP_CREATE_SUCCESS:

                printf("\t%sAnnouncement:%s%s", ANNOUNCE, RESET, package.other);

                break;
            case GROUP_CREATE_FAIL:

                printf("\t%sWarning:%s%s", WARNING, RESET, package.other);

                break;
            case JOIN_GROUP_SUCCESS:

                printf("\t%sAnnouncement:%s%s", ANNOUNCE, RESET, package.other);
                status = JOIN_GROUP;
                strcpy(clientInfo.to, package.groupName);

                break;
            case JOIN_GROUP_FAIL:

                printf("\t%sWarning:%s%s", WARNING, RESET, package.other);
                status = FREE;

                break;
            case JOIN_USERNAME_SUCCESS:

                status = JOIN_USERNAME;
                enum json_type type;
                json_object *jobj = json_tokener_parse(package.jObject);
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
                if (strcmp(clientInfo.from, message.from) == 0)
                {
                    strcpy(clientInfo.to, message.to);
                }
                else
                    strcpy(clientInfo.to, message.from);
                printf("\n\t%sInformation:%s%s\n", ANNOUNCE, RESET, message.message);
                break;
            case JOIN_USERNAME_FAIL:

                printf("\n\t%sWarning:%s%s", WARNING, RESET, package.other);
                status = FREE;

                break;
            case EXIT_SUCCESSFUL:

                printf("\n\t%sAnnouncement:%s%s", ANNOUNCE, RESET, package.other);
                isExit = 1;

                break;
            case EXIT_GROUP_SUCCESS:

                printf("\t%sAnnouncement:%s%s", ANNOUNCE, RESET, package.other);
                status = FREE;
                strcpy(clientInfo.to, "");

                break;
            case EXIT_GROUP_FAIL:

                printf("\t%sWarning:%s%s", WARNING, RESET, package.other);

                break;
            case WHOAMI:

                printf("%s%s%s", YELLOW, package.other, RESET);

                break;
            case MESSAGE:
                Message(package);
                break;
            case MESSAGE_USER:
                MessagePrivate(package);
                break;
            default:
                break;
            }
            pthread_mutex_unlock(&clients_mutex);
        }
    }
    catch_crtl_c_and_exit(2);
}

void SendHandler()
{
    char buffer[BUFFER_SZ] = {};
    char message[BUFFER_SZ + NAME_SIZE] = {};
    int isFail = 0;
    while (1)
    {

        OverwriteStdout();
        fgets(buffer, BUFFER_SZ, stdin);

        char temp[BUFFER_SZ];
        strcpy(temp, buffer);
        Trim(temp, strlen(temp));

        int size = strlen(temp);
        char *cmd = strtok(buffer, " ");
        Trim(cmd, strlen(cmd));
        package_t package;
        if (strcmp(cmd, "-gcreate") == 0) // -gcreate
        {
            //package_t package;
            if (status == JOIN_GROUP)
            {
                printf("You already joined a group.");
                printf("You do not use %s command before use '-exit group_name'\n", cmd);
                isFail = 0;
                continue;
            }
            pthread_mutex_lock(&clients_mutex);
            package.type = GROUP_CREATE;
            cmd = strtok(NULL, " ");
            Trim(cmd, strlen(cmd));
            strcpy(package.other, cmd);
            cmd = strtok(NULL, "\0");
            Trim(cmd, strlen(cmd));
            strcpy(package.groupName, cmd);
            printf("Password for group '%s': ", package.groupName);
            char pass[PASS_SIZE];
            fgets(pass, PASS_SIZE, stdin);
            Trim(pass, strlen(pass));
            strcpy(package.password, pass);
            pthread_mutex_unlock(&clients_mutex);
            isFail = 1;
        }
        else if (strcmp(cmd, "-join") == 0) // -join
        {
            //package_t package;
            if (status == JOIN_GROUP)
            {
                printf("You already joined a group.");
                printf("You do not use %s command before use '-exit group_name'\n", cmd);
                isFail = 0;
                continue;
            }
            pthread_mutex_lock(&clients_mutex);
            cmd = strtok(NULL, "\0");
            Trim(cmd, strlen(cmd));
            strcpy(package.groupName, cmd);
            strcpy(package.other, clientInfo.from);
            printf("If '%s' is a group, enter password, otherwise press enter: ", package.groupName);
            char pass[PASS_SIZE];
            fgets(pass, PASS_SIZE, stdin);
            if (strcmp(pass, "\n") == 0)
            {
                package.type = JOIN_USERNAME;
            }
            else
            {
                package.type = JOIN_GROUP;
                Trim(pass, strlen(pass));
                strcpy(package.password, pass);
            }
            pthread_mutex_unlock(&clients_mutex);
            isFail = 1;
        }
        else if (strcmp(cmd, "-send") == 0) // -send
        {
            //package_t package;
            pthread_mutex_lock(&clients_mutex);
            if (status == JOIN_GROUP)
                package.type = SEND_GROUP;
            else if (status == JOIN_USERNAME)
                package.type = SEND_USERNAME;
            else
            {
                printf("You did not join to any group or username!\nIf you want to send a message, firstly join to a group or username.\n");
                isFail = 0;
                continue;
            }
            cmd = strtok(NULL, "\0");
            Trim(cmd, strlen(cmd));
            message_t messg;
            strcpy(messg.from, clientInfo.from);
            strcpy(messg.to, clientInfo.to);
            strcpy(messg.message, cmd);
            json_object *obj = convertToJSONObject(messg);
            strcpy(package.jObject, json_object_get_string(obj));
            pthread_mutex_unlock(&clients_mutex);
            isFail = 1;
        }
        else if (strcmp(cmd, "-whoami") == 0) // -whoami
        {
            pthread_mutex_lock(&clients_mutex);
            //package_t package;
            package.type = WHOAMI;
            pthread_mutex_unlock(&clients_mutex);
            isFail = 1;
        }
        else if (strcmp(cmd, "-exit") == 0) // -exit
        {
            //package_t package;
            //int size=strlen(buffer);
            pthread_mutex_lock(&clients_mutex);
            if (size == 5) //-exit
            {
                if (status == JOIN_GROUP)
                {
                    printf("You already joined a group.");
                    printf("You do not use %s command\nbefore use '-exit group_name'\n", cmd);
                    isFail = 0;
                    continue;
                }
                package.type = EXIT;
                isFail = 1;
            }
            else //-exit group
            {
                cmd = strtok(NULL, "\n");
                Trim(cmd, strlen(cmd));
                package.type = EXIT_GROUP;
                strcpy(package.other, cmd);
                isFail = 1;
            }
            pthread_mutex_unlock(&clients_mutex);
        }
        else if (strcmp(cmd, "-help") == 0)
        {
            pthread_mutex_lock(&clients_mutex);
            PrintScreen();
            pthread_mutex_unlock(&clients_mutex);
            isFail = 0;
        }
        else // wrong command
        {
            pthread_mutex_lock(&clients_mutex);
            printf("Wrong command '%s'\n",cmd);
	    printf("For usage of command, write '-help'\n");
            printf("\n\n");
            pthread_mutex_unlock(&clients_mutex);
            isFail = 0;
        }

        if (isFail == 1)
        {
            if (send(sockfd, &package, sizeof(package_t), 0) < 0)
            {
                perror("Send failed");
                exit(1);
            }
        }

        bzero(buffer, BUFFER_SZ);
        
    }
    catch_crtl_c_and_exit(2);
}

void Message(package_t p)
{
    enum json_type type;
    message_t message;
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
    printf("\t\t%s%s%s: %s\n", BLUE, message.from, RESET, message.message);
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

void jsonParse(json_object *jobj, message_t message)
{
    enum json_type type;
    json_object_object_foreach(jobj, key, val)
    {
        type = json_object_get_type(val);
        switch (type)
        {
        case json_type_string:
            if (strcmp(key, "from") == 0)
            {
                strcpy(message.from, json_object_get_string(val));
            }
            else if (strcmp(key, "to") == 0)
            {
                strcpy(message.to, json_object_get_string(val));
            }
            else if (strcmp(key, "message") == 0)
            {
                strcpy(message.message, json_object_get_string(val));
            }
            break;
        }
    }
}

void PrintScreen() /*  Command Menu  */
{
    printf("\t\t\tUsage of Commands\n\n");
    printf("    1- -greate phone_number group_name\t=> Create a new specified group.\n");
    printf("    2- -join username/group_name\t=> Join to a group\n");
    printf("    3- -send message_body\t\t=> Send message to group\n");
    printf("    4- -whoami\t\t\t\t=> Who am I?\n");
    printf("    5- -help\t\t\t\t=> Help for commands\n");
    printf("    6- -exit group_name\t\t\t=> Exit from group\n");
    printf("    7- -exit\t\t\t\t=> Exit the program.\n");
    printf("--------------------------------------------------------------------------\n");
}

void MessagePrivate(package_t p)
{
    enum json_type type;
    message_t message;
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
    printf("\t\t%s%s%s: %s\n", BLUE, message.from, RESET, message.message);
}
