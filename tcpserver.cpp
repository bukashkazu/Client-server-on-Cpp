#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>
#pragma comment(lib, "ws2_32.lib") 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define TRUE   1  
#define FALSE  0  
#define BACKLOG 110 // максимальное количество сокетов для подключения одновременно , в listen()
#define MAX_CLIENTS 110
#define SIZE_PORT 20
#define MAX_SIZE 350824
#define TIMEOUT -5

#define PUT_SIZE 3
#define PUT_RECIEVED 3
#define PUT_NOT_RECIEVED 0

#define CLIENT_DISCONECTED -1
#define RECV_IS_NORMAL 0
#define RECV_ISNOT_NORMAL -2

#define FILENAME "msg.txt"


void check_argc(int argc);
int read_argv(char** argv);
int init();
void init_socket(struct sockaddr_in* addr, int port);
void set_reuseaddr(int socket);
void check_bind(int n);
void check_listen(int n);
int set_non_block_mode(int s);
void deinit();
void error(const char* msg);
void check_socket(int s_desc);
void check_file(FILE* infile);
int set_non_block_mode(int s);
void init_clients_socket(struct Node* client_fd);
int check_select(int s);
void check_accept(int a);
void put_new_socket_to_array(struct Node* clients_fd, SOCKET new_socket, struct sockaddr_in* addr);
int check_recv(int r);
int recieve_put(struct Node client, char* buf, int* flag);
void form_port_and_adr_msg(struct Node client, char* begin);
void transform_to_date(uint32_t bytes, char* date);
int fullfill_str(char* buffer, char* sms, int offset);
void recieve_msg(struct Node client, char* port_and_addr, char* buf, int offset);

struct Node {
    SOCKET socket;
    struct sockaddr_in addr;
    int msg_count;
    int count_of_ok;
    int put_recieved;
    struct Node* next;
};
int flag_stop = FALSE;
int active_clients = 0;
FILE* file = NULL;

int main(int argc, char* argv[])
{
    check_argc(argc);
    
    int port = read_argv(argv);
    
    init();

    SOCKET main_socket = socket(AF_INET, SOCK_STREAM, 0); 
    check_socket(main_socket);
    set_reuseaddr(main_socket);

    struct sockaddr_in addr;
    init_socket(&addr, port);
    
    int b = bind(main_socket, (struct sockaddr*)&addr, sizeof(addr));
    check_bind(b);

    int l = listen(main_socket, BACKLOG);
    check_listen(l);

    set_non_block_mode(main_socket);

    fd_set readfd;
    SOCKET max_fd;
    struct timeval timeout;
    struct Node clients_fd[MAX_CLIENTS];
    init_clients_socket(clients_fd);

    int flag_client_disconected = RECV_IS_NORMAL;
    file = fopen(FILENAME, "w+");
    check_file(file);

    while (flag_stop==FALSE)
    {
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        FD_ZERO(&readfd);
        FD_SET(main_socket, &readfd);
        max_fd = main_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients_fd[i].socket > 0) {
                FD_SET(clients_fd[i].socket, &readfd);
            }
            if (clients_fd[i].socket > max_fd) {
                max_fd = clients_fd[i].socket;
            }
        }
        int s = select(max_fd + 1, &readfd, NULL, NULL, &timeout);
        if(check_select(s) == TIMEOUT)continue;

        if (FD_ISSET(main_socket, &readfd)) // если событие есть
        {
            struct sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            SOCKET client_socket = accept(main_socket, (struct sockaddr*)&client_addr, &addr_len);
            check_accept(client_socket);
            set_non_block_mode(client_socket);
            put_new_socket_to_array(clients_fd, client_socket, &client_addr);
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (FD_ISSET(clients_fd[i].socket, &readfd) && clients_fd[i].socket!=0)
            {
                fprintf(stdout, "Recieving msg from client %d\n", i);
                char port_and_addr[50] = { '\0' };
                form_port_and_adr_msg(clients_fd[i], port_and_addr);

                char* buf = (char*)malloc(MAX_SIZE);
                int ra = recieve_put(clients_fd[i], buf, &flag_client_disconected);
                if (ra == PUT_RECIEVED || clients_fd[i].put_recieved == PUT_RECIEVED)
                {
                    clients_fd[i].put_recieved = PUT_RECIEVED;
                    recieve_msg(clients_fd[i], port_and_addr, buf, ra);


                    if (flag_client_disconected == CLIENT_DISCONECTED)
                    {
                        fprintf(stdout, "Client %d disconected", i);
                        FD_CLR(clients_fd[i].socket, &main_socket);
                        closesocket(clients_fd[i].socket);
                        clients_fd[i].socket = 0;
                    }
                }
                
            }
        }

    }

    for (int j = 0; j < MAX_CLIENTS; j++) {
        if (clients_fd[j].socket != 0) {
            closesocket(clients_fd[j].socket);
            clients_fd[j].socket = 0;
        }
    }
    closesocket(main_socket);
    fclose(file);
    deinit();
    return 0;
}

void recieve_msg(struct Node client, char* port_and_addr, char* buf, int offset) 
{
    int r = 1;
    char* sms = (char*)malloc(MAX_SIZE);
    if (sms)
    {
        memset(sms, 0x00, MAX_SIZE);
        strcpy(sms, port_and_addr);
        flag_stop = fullfill_str(buf, sms, offset);
        if(strlen(sms)>37)fprintf(file, "%s\n", sms);
        send(client.socket, "ok", 2, 0);
    }
    free(sms);
}

int fullfill_str(char* buffer, char*sms, int offset)
{
    int all_msg_len = offset; 
    char phone[13] = { '\0' }, date[20] = { '\0' };
    char*message = (char*)malloc(MAX_SIZE);
    if (message)memset(message, 0x00, MAX_SIZE);
    uint32_t num_msg_32, timestamp, msg_len_32;

    memcpy(&num_msg_32, &buffer[all_msg_len], 4);
    all_msg_len += 4;
    memcpy(&timestamp, &buffer[all_msg_len], 4);
    all_msg_len += 4;
    memcpy(&phone, &buffer[all_msg_len], 12);
    all_msg_len += 12;
    memcpy(&msg_len_32, &buffer[all_msg_len], 4);
    all_msg_len += 4;
    if (message)strcpy(message, &buffer[all_msg_len]);

    int msg_num = ntohl(num_msg_32);
    transform_to_date(timestamp, date);
    int msg_len = ntohl(msg_len_32);


    char* tmp = (char*)malloc(MAX_SIZE);
    if (tmp)
    {
        memset(tmp, 0x00, MAX_SIZE);
        sprintf(tmp, "%s %s %s", date, phone, message);
        strcat(sms, tmp);
        if (strcmp("stop", message) == 0)
        {
            free(message);
            return 1;
        }
    }
    free(message);
    return 0;
    
}

void transform_to_date(uint32_t bytes, char* date) 
{
    uint32_t unix_time = ntohl(bytes);
    time_t timestamp = unix_time;
    struct tm* tm = localtime(&timestamp);
    tm->tm_isdst = -1;
    tm->tm_year += 1900;
    tm->tm_mon += 1;
    sprintf(date, "%02d.%02d.%d %02d:%02d:%02d", tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void deinit()
{
    WSACleanup();
}

int recieve_put(struct Node client, char* buf, int* flag)
{
    memset(buf, 0x00, MAX_SIZE);
    int r = recv(client.socket, buf, MAX_SIZE, 0);
    (*flag) = check_recv(r);
    if (sizeof(buf) < 3)(*flag) = CLIENT_DISCONECTED;

    if (strncmp("put", buf, 3) == 0)
    {
        return PUT_RECIEVED;
    }
    else return PUT_NOT_RECIEVED;
}

int check_recv(int r)
{
    if (r == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            error("Socket is not ready for reading");
        }
        else {
            return CLIENT_DISCONECTED;
        }
    }
    else if (r == 0) {
        // client disconected 
        active_clients--;
        fprintf(stdout, "Client in check_recv disconected\n");
        return CLIENT_DISCONECTED;
    }
    else return RECV_IS_NORMAL;
}

void check_file(FILE* infile)
{
    if (infile == NULL)
    {
        error("Unable to open input file");
    }
}

void form_port_and_adr_msg(struct Node client, char* begin)
{
    char num[20] = { '\0' };
    strcat(begin, inet_ntoa(client.addr.sin_addr));
    strcat(begin, ":");
    sprintf(num, "%d ", ntohs(client.addr.sin_port));
    strcat(begin, num);
}

void put_new_socket_to_array(struct Node* clients_fd, SOCKET new_socket, struct sockaddr_in* addr)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients_fd[i].socket == 0) {
            clients_fd[i].socket = new_socket;
            clients_fd[i].addr = (*addr);
            active_clients++;
            break;
        }
    }
}

void check_accept(int a)
{
    if (a < 0 || a == INVALID_SOCKET)error("Accept() failed");
}

int check_select(int s)
{
    if (s < 0)error("Error in select()");
    else if (s == 0)return TIMEOUT;
    return 1;
}

void init_clients_socket(struct Node* client_fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fd[i].socket = 0;
        client_fd[i].msg_count = 0;
        client_fd[i].count_of_ok = 0;
    }
}

int set_non_block_mode(int s)
{
    unsigned long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode);
}

void check_bind(int n)
{
    if (n < 0)error("Error in BIND");
}

void check_listen(int n)
{
    if (n < 0)error("Error in listen");
}

void set_reuseaddr(int socket)
{
    //Установка опции SO_REUSEADDR
    int optval = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR) 
    {
        error("Error establishing SO_REUSEADDR");
    }
 }

void init_socket(struct sockaddr_in* addr, int port)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;
}

void check_socket(int s_desc)
{
    if (s_desc < 0)
    {
        error("Could not create socket. Please try again.");
    }
}

int init()
{
    WSADATA wsa_data;
    return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
}

int read_argv(char** argv)
{
    if (argv[1] == NULL)error("Invalid server addr or port");
    int port = 0;
    if(argv[1])port = atoi(argv[1]);

    return port;
}

void check_argc(int argc)
{
    if (argc != 2)
    {
        error("Invalid count of arguments. Enter port and try again.");
    }
}

void error(const char* msg)
{
    fprintf(stdout, "%s: %d\n", msg, WSAGetLastError());
    exit(EXIT_FAILURE);
}

