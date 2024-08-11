#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> 
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <poll.h>
#include <time.h>


#define SIZE_SOCKET_ARR 10
#define COUNT_MAIN_SOCKETS 100
#define MAX_SOCKET 1024
#define SIZE_BUFFER 5000
#define MAX_MSG 20

int sock_err(const char* function, int s);
void check_argc(int argc);
void error(const char *msg);
void read_argv(int argc, char** argv,  char* port_1, char* port_2);
void check_socket(int s_desc);
void init_socket(sockaddr_in* addr, int port);
int set_non_block_mode(int s);
void check_file(FILE* infile);
void check_poll(int ev_cnt);
void create_sockets(int* sockets_arr, int begin, int end, struct sockaddr_in* server_addr, struct pollfd* pfd);
void transform_to_date(uint32_t bytes, char* date);
void fullfill_str(char* buffer, char* res_str, int* num, int*len, int* flag_end);
int check_msg_num(int* recieved_msg, int num);
void send_answer(int* recieved, int fd, struct sockaddr_in* addr, int addrlen, int index);

int main(int argc, char** argv)
{
    check_argc(argc);
    char* server_port_one = (char*)malloc(SIZE_SOCKET_ARR);
    char* server_port_two = (char*)malloc(SIZE_SOCKET_ARR);
    read_argv(argc, argv, server_port_one, server_port_two);
    int port_1 = atoi(server_port_one), port_2 = atoi(server_port_two);

    struct sockaddr_in server_addr;
    struct pollfd pfd[MAX_SOCKET];
    init_socket(&server_addr, port_1);
    int addrlen = sizeof(server_addr);
    int s[COUNT_MAIN_SOCKETS]= {0};
    create_sockets(s, port_1, port_2, &server_addr, pfd);


    FILE* file = fopen("msg.txt", "w+");
    check_file(file);

    struct sockaddr_in clients[MAX_SOCKET];
    int num_clients = 0, flag_end = 0, recieved_msg[MAX_MSG], index = 0;
    for(int i = 0; i <MAX_MSG; i++)recieved_msg[i]= -1;

    fprintf(stdout, "Server started. Listening on ports...\n");

    do
    {
        int ev_cnt = poll(pfd, sizeof(pfd) / sizeof(pfd[0]), 1000);
        check_poll(ev_cnt);
        for(int j = 0; j<index+1;j++)
                {
                    recieved_msg[j] = -1;
                    
                }
                index = 0;
        for (int i = 0; i <= port_2 - port_1; i++)
        {
            if (pfd[i].revents & POLLHUP)
            {
                for(int j = 0; j<index+1;j++)
                {
                    recieved_msg[j] = 0;
                    
                }
                index = 0;
                close(pfd[i].fd);
            }
            if (pfd[i].revents & POLLERR)
            {
                close(pfd[i].fd);
            }
            if (pfd[i].revents & POLLIN)
            {
                char buffer[SIZE_BUFFER] = {'\0'};
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                ssize_t bytes = recvfrom(pfd[i].fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
                if (bytes == -1)error("ERROR in recv");
                else 
                {
                    char client_id[INET_ADDRSTRLEN] = {'\0'}; 
                    char client_port[10] = {'\0'};
                    char begin[20] = {'\0'};
                    sprintf(client_id, "%s", inet_ntoa(client_addr.sin_addr));
                    sprintf(client_port, "%d", ntohs(client_addr.sin_port));
                    sprintf(begin, "%s:%s ", client_id, client_port);

                    int client_index = -1;
                    for (int j = 0; j < num_clients; j++) {
                        if (strcmp(client_id, inet_ntoa(clients[j].sin_addr)) == 0 && ntohs(client_addr.sin_port) == ntohs(clients[j].sin_port))
                        {
                            client_index = j;
                            break;
                        }
                    }

                    if (client_index == -1) 
                    {
                        if (num_clients >= MAX_SOCKET) 
                        {
                            fprintf(stdout, "Maximum number of clients reached\n");
                            continue;
                        }
                        client_index = num_clients++;
                        clients[client_index] = client_addr;
                    } 
     
                    char res_str[SIZE_BUFFER] = { '\0' };
                    int msg_num = 0, msg_len = 0;
                    fullfill_str(buffer, res_str, &msg_num, &msg_len, &flag_end);
                    if(check_msg_num(recieved_msg, msg_num)==0)
                    {
                        //printf("Enetered to SENDING...\n");
                        recieved_msg[index] = msg_num;
                        index++;
                        fprintf(file, "%s", begin);
                        fprintf(file, "%s\n", res_str);
                        //printf("Received message from %s:%d: %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), res_str);
                        send_answer(recieved_msg, pfd[i].fd, &clients[client_index], client_len, index);
                        if(flag_end==1)break;
                    }
                    else continue;
                    
                }
            }
        }
        if(flag_end == 1)break;
    } while (1);
    
    return 0;
}
void send_answer(int* recieved, int fd, struct sockaddr_in* addr, int addrlen, int index)
{
    uint32_t copy[MAX_MSG];
    char datagram[80] = {'\0'};
    int all_msg_len = 0;
    for(int i = 0; i< index+1;i++)
    {
        if(recieved[i]!=-1)copy[i] = htonl(recieved[i]);
        memcpy(&datagram[all_msg_len], &copy[i], 4);
        all_msg_len += 4;
    }
    //printf("SIZE OF DATA IS:  %d\n\n", all_msg_len);
    sendto(fd, datagram, all_msg_len,MSG_NOSIGNAL, (struct sockaddr*)addr, addrlen);
}
int check_msg_num(int* recieved_msg, int num)
{
    for(int i = 0; i< MAX_MSG; i++)
    {
        if(recieved_msg[i]==num)return 1;

    }
    return 0;
}
void fullfill_str(char* buffer, char* res_str, int* num, int*len, int* flag_end)
{
    int all_msg_len = 0;
    char phone[13] = { '\0' }, date[20] = { '\0' }, message[SIZE_BUFFER] = { '\0' };
    uint32_t num_msg_32, timestamp, msg_len_32;

    memcpy(&num_msg_32, &buffer[all_msg_len], 4);
    all_msg_len += 4;
    memcpy(&timestamp, &buffer[all_msg_len], 4);
    all_msg_len += 4;
    memcpy(&phone, &buffer[all_msg_len], 12);
    all_msg_len += 12;
    memcpy(&msg_len_32, &buffer[all_msg_len], 4);
    all_msg_len += 4;
    strcpy(message, &buffer[all_msg_len]);

    int msg_num = ntohl(num_msg_32);
    transform_to_date(timestamp, date);
    int msg_len = ntohl(msg_len_32);
    (*num) = msg_num;
    (*len) = msg_len;
    sprintf(res_str, "%s %s %s", date, phone, message);
    if (strcmp(message, "stop") == 0)(*flag_end) = 1;
}
void transform_to_date(uint32_t bytes, char* date) {
    uint32_t unixTime = ntohl(bytes); 
    time_t timestamp = unixTime;
    //struct tm* tm = gmtime(&timestamp);
    struct tm* tm = localtime(&timestamp);
    tm->tm_isdst = -1;
    tm->tm_year += 1900;
    tm->tm_mon += 1;
    sprintf(date, "%02d.%02d.%d %02d:%02d:%02d", tm->tm_mday, tm->tm_mon, tm->tm_year,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
}
void check_poll(int ev_cnt)
{
    if(ev_cnt<0)
    {
        error("Timeout or error in POLL");
    }
}
void create_sockets(int* sockets_arr, int begin, int end, struct sockaddr_in* server_addr, struct pollfd* pfd)
{
    int counter = 0;
    for (int i = begin; i <= end; i++)
    {
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        check_socket(s);
        set_non_block_mode(s);
        sockets_arr[counter] = s;
        

        server_addr->sin_port = htons(i);

        if (bind(s, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)error("Smth happend in BIND");
        
        pfd[counter].fd = s;
        pfd[counter].events = POLLIN;// | POLLOUT;
        counter++;
    }
}
int sock_err(const char* function, int s)
{
    int err;
    err = errno;
    fprintf(stderr, "%s: socket error: %d\n", function, err);
    return -1;
}
void read_argv(int argc, char** argv,  char* port_1, char* port_2)
{
     
    memset(port_1, 0x00, sizeof(port_1));
    memset(port_2, 0x00, sizeof(port_2));

    strcpy(port_1, argv[1]);
    strcpy(port_2, argv[2]);
    
    if (port_1 == NULL || port_2 == NULL) {
        error("Invalid server ports\n");
    }
}
void check_argc(int argc)
{
    if(argc!=3)
    {
        printf("Invalid count of arguments. Enter port, address, file name and try again\n");
        exit(EXIT_FAILURE);
    }
}
void error(const char *msg)
{
    fprintf(stdout, "%s", msg);
    exit(EXIT_FAILURE);
}
void check_socket(int s_desc)
{
    if (s_desc < 0)
	{
		error("Could not create socket. Please try again.");
	}
}
void init_socket(sockaddr_in* addr, int port)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr =  INADDR_ANY;
}
int set_non_block_mode(int s)
{
    int fl = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, fl | O_NONBLOCK);
    }
void check_file(FILE* infile)
{
    if (infile == NULL) {
        error("Unable to open input file\n");
    }
}