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
#define ADDR_PORT_SIZE 30
#define MAX_SIZE 341000
#define RECV_SIZE 2048//4096
#define TIMEOUT -5

#define PUT_RECIEVED 3
#define PUT_NOT_RECIEVED 0

#define CLIENT_DISCONECTED -1
#define CLIENT_CONNECTED 0
#define RECV_IS_NORMAL 0
#define RECV_ISNOT_NORMAL -2

#define INCORRECT_FORMAT -10
#define CORRECT_FORMAT 10

#define FILENAME "msg.txt"

#define NOT_USE 9
void check_argc(int argc);
int read_argv(int argc, char** argv);
int init();
void init_struct_addr(struct sockaddr_in* addr, int port);
void check_bind(int n);
void check_listen(int n);
int set_non_block_mode(int s);
void deinit();
void error(const char* msg);
void check_socket(int s_desc);
void check_file(FILE* infile);
void init_clients_socket(struct Node* client_fd);
int check_select(int s);
void check_accept(int a);
void put_new_socket_to_array(struct Node* client, SOCKET new_socket, struct sockaddr_in* addr);
int check_recv(int r);
void recieve_msg(struct Node* client);
void form_port_and_adr_msg(struct Node client, char* begin);
void transform_to_date(uint32_t bytes, char* date);
int fullfill_str(char* buffer, char* sms);
void prepare_msg_and_send(struct Node* client);

int check_corectness(char* message)
{
    int flag = FALSE;
    if (message[2] != '.' || message[5] != '.' || message[10] != ' ' ||
        !isdigit(message[0]) || !isdigit(message[1]) || !isdigit(message[3]) || !isdigit(message[4]) ||
        !isdigit(message[6]) || !isdigit(message[7]) || !isdigit(message[8]) || !isdigit(message[9]))
    {
        flag = TRUE;
        fprintf(stdout, "Condition 1\n %c", message[2]);
    }
    else if (message[13] != ':' || message[16] != ':' || !isdigit(message[11]) || !isdigit(message[12]) || !isdigit(message[14]) || !isdigit(message[15]) || !isdigit(message[17]) || !isdigit(message[18]))
    {
        flag = TRUE;
        fprintf(stdout, "Condition 2\n");
    }
    else if (message[20] != '+' || message[21] != '7' || !isdigit(message[22]) || !isdigit(message[23]) || !isdigit(message[23]) || !isdigit(message[25]) || !isdigit(message[26]) || !isdigit(message[27]) || !isdigit(message[28]) || !isdigit(message[29]) || !isdigit(message[30]))
    {
        flag = TRUE;
        fprintf(stdout, "Condition 3\n %s", message);
    }
    else if (message[31] == '\0')
    {
        flag = TRUE;
        fprintf(stdout, "Condition 4\n");
    }

    if (flag == TRUE)return INCORRECT_FORMAT;
    else return CORRECT_FORMAT;
}


struct Node 
{
    SOCKET socket;
    struct sockaddr_in addr;
    int put_recieved;
    int msg_is_ready;
    char* buf_bytes; 
    int count_of_bytes_recieved;
    int msg_len;
    int flag_client_disconected;
};

int delete_client(struct Node* arr, int active_clients, int indexToRemove) {
    if (indexToRemove < 0 || indexToRemove >= active_clients) {
        return active_clients;
    }

    if (arr[indexToRemove].buf_bytes != NULL) {
        free(arr[indexToRemove].buf_bytes);
        arr[indexToRemove].buf_bytes = (char*)calloc(MAX_SIZE, 1);
    }

    if (indexToRemove < active_clients - 1) {
        memmove(&arr[indexToRemove], &arr[indexToRemove + 1], (active_clients - indexToRemove - 1) * sizeof(struct Node));
    }

    arr[active_clients - 1].socket = 0;
    arr[active_clients - 1].count_of_bytes_recieved = 0;
    arr[active_clients - 1].msg_is_ready = FALSE;
    arr[active_clients - 1].put_recieved = PUT_NOT_RECIEVED;
    arr[active_clients - 1].buf_bytes = (char*)calloc(MAX_SIZE, 1);
    arr[active_clients - 1].msg_len = 0;
    arr[active_clients - 1].flag_client_disconected = CLIENT_CONNECTED;

    return active_clients - 1;
}

int flag_stop = FALSE;
int active_clients = 0;
FILE* file = NULL;

void reuseaddr(int main_socket)
{
    int optival = 1;
    setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&optival, sizeof(optival));
}

int main(int argc, char* argv[])
{
    int port = read_argv(argc, argv);

    init();
    SOCKET main_socket = socket(AF_INET, SOCK_STREAM, 0);
    check_socket(main_socket);
    
    reuseaddr(main_socket);
    set_non_block_mode(main_socket); 
    
    struct sockaddr_in addr;
    init_struct_addr(&addr, port);

    int b = bind(main_socket, (struct sockaddr*)&addr, sizeof(addr));
    check_bind(b);

    int l = listen(main_socket, BACKLOG);
    check_listen(l);

    struct Node client[MAX_CLIENTS];
    init_clients_socket(client);

    fd_set readfd;
    SOCKET max_fd;
    struct timeval timeout;
    

    file = fopen(FILENAME, "w");
    check_file(file);

    while (flag_stop == FALSE)
    {
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        FD_ZERO(&readfd);
        FD_SET(main_socket, &readfd);
        max_fd = main_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) 
        {
            if (client[i].socket > 0) 
            {
                FD_SET(client[i].socket, &readfd);
               
            }
            if (client[i].socket > max_fd) {
                max_fd = client[i].socket;
            }
            
        }
        
        int s = select(max_fd + 1, &readfd, NULL, NULL, &timeout); 
        if (check_select(s) == TIMEOUT) {continue; }

        if (FD_ISSET(main_socket, &readfd)) 
        {
            struct sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            SOCKET client_socket = accept(main_socket, (struct sockaddr*)&client_addr, &addr_len);
            check_accept(client_socket);
            put_new_socket_to_array(client, client_socket, &client_addr);
        }

        for (int i = 0; i < active_clients; i++)
        {
            if (FD_ISSET(client[i].socket, &readfd) && client[i].socket != 0)
            {
                fprintf(stdout, "Recieving msg from client %d\n", i);

                recieve_msg(&client[i]);
                if (client[i].flag_client_disconected == CLIENT_DISCONECTED)
                {
                    fprintf(stdout, "Client %d disconnected and removed\n\n", client[i].socket);

                    closesocket(client[i].socket);

                    active_clients = delete_client(client, active_clients, i);

                }
                else if (client[i].put_recieved == PUT_RECIEVED && client[i].msg_is_ready == TRUE)
                {
                    prepare_msg_and_send(&client[i]);
                }
                
                
            }
        }
    }
    for (int j = 0; j < MAX_CLIENTS; j++)
    {
        if (client[j].socket != 0) {
            closesocket(client[j].socket);
            client[j].socket = 0;
        }
    }
    closesocket(main_socket);
    fclose(file);
    deinit();
    return 0;
}

void prepare_msg_and_send(struct Node* client)
{
    char* port_and_addr = (char*)calloc(ADDR_PORT_SIZE, 1);
    form_port_and_adr_msg((*client), port_and_addr);

    char* sms = (char*)calloc(MAX_SIZE, 1);
    if (sms && port_and_addr)
    {
        strcpy(sms, port_and_addr);
        flag_stop = fullfill_str((*client).buf_bytes, sms);
        if (flag_stop == INCORRECT_FORMAT)
        {
           // fprintf(stdout, "INCOR\n");
            flag_stop = FALSE;
            send((*client).socket, "ok", 2, 0);
        }
        else if (strlen(sms) > 30)
        {
           /* fprintf(stdout, "COR\n");*/
            fprintf(file, "%s\n", sms);
            send((*client).socket, "ok", 2, 0);
            memset(client->buf_bytes, 0x00, MAX_SIZE);
            client->count_of_bytes_recieved = 0;
            client->msg_len = 0;
            client->msg_is_ready = FALSE;
        }

    }
}

int fullfill_str(char* buffer, char* sms)
{
    int all_msg_len = 0;
    char phone[13] = { '\0' }, date[20] = { '\0' };
    char* message = (char*)calloc(MAX_SIZE, 1);

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


    char* tmp = (char*)calloc(MAX_SIZE, 1);
    if (tmp)
    {
        sprintf(tmp, "%s %s %s", date, phone, message);
        if (strlen(tmp) > 27)
        {
            if (check_corectness(tmp) == CORRECT_FORMAT)
            {
                strcat(sms, tmp);
                if (strcmp("stop", message) == 0)
                {
                    free(message);
                    return 1;
                }
            }
            else return INCORRECT_FORMAT;
        }

    }
    free(message);
    free(tmp);
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

void recieve_msg(struct Node* client)
{

    int r = recv(client->socket, &(client->buf_bytes[client->count_of_bytes_recieved]), RECV_SIZE, 0);
    client->count_of_bytes_recieved += r;
    client->flag_client_disconected = check_recv(r);
    
    if (client->flag_client_disconected == CLIENT_DISCONECTED) {
        return;
    }
   
    if (client->msg_len == 0)
    {
        uint32_t msg_len_32 = 0;
        if (client->count_of_bytes_recieved >= 24)
        {
            memcpy(&msg_len_32, &client->buf_bytes[20], 4);
        }
        int msg_len = ntohl(msg_len_32);
        client->msg_len = msg_len;
    }

    if (strncmp("put", client->buf_bytes, 3) == 0)
    {
        client->put_recieved = PUT_RECIEVED;
        //client->buf_bytes += 3;
        memmove(client->buf_bytes, client->buf_bytes + 3, client->count_of_bytes_recieved - 3);
        client->count_of_bytes_recieved -= 3;
    }

    if (client->count_of_bytes_recieved == client->msg_len + 24)
    {
        fprintf(stdout, "Message ready\n");
        client->msg_is_ready = TRUE;
    }
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
    else if (r <= 0) {
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

void put_new_socket_to_array(struct Node* client, SOCKET new_socket, struct sockaddr_in* addr)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client[i].socket == 0)
        {
            client[i].socket = new_socket;
            client[i].addr = (*addr);
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
    else if (s == 0) {fprintf(stdout, "TIMEOUT IN SELECT"); return TIMEOUT; }
    return 1;
}

void init_clients_socket(struct Node* client_fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_fd[i].socket = 0;
        client_fd[i].count_of_bytes_recieved = 0;
        client_fd[i].msg_is_ready = FALSE;
        client_fd[i].put_recieved = PUT_NOT_RECIEVED;
        client_fd[i].buf_bytes = (char*)calloc(MAX_SIZE, 1);
        client_fd[i].msg_len = 0;
        client_fd[i].flag_client_disconected = CLIENT_CONNECTED;
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


void init_struct_addr(struct sockaddr_in* addr, int port)
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

int read_argv(int argc, char** argv)
{
    check_argc(argc);

    if (argv[1] == NULL)error("Invalid server addr or port");
    int port = 0;
    if (argv[1])port = atoi(argv[1]);

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

