#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>

#define MAX_ATTEMPTS 10
#define N 20
#define WAIT_TIME_MS 100
#define MAX_SIZE 350824
#define NO_CONNECTION 0
#define CONNECTED 1
#define OK_RECIEVED 1
#define OK_NOT_RECIEVED 0
#define FILENAME_SIZE 100

void error(const char *msg);
void check_file(FILE* file);
void check_argc(int argc);
void read_argv(int argc, char** argv,  char* port, char* adr, char* file_name);
void check_null(char*);
void check_sockfd(int sockfd);
void check_connection(int flag);
void sent_get_msg(int sockfd);
int read_msgs_from_server(int sock, char* filename, char* addr, int port);
void transform_to_date(uint32_t bytes, char* date);
void fullfill_str(char* buffer, char* res_str);

int main(int argc, char *argv[])
 {
    check_argc(argc);

    char* ServerPort_str = (char*)malloc(N);
    char* ServerAddr = (char*)malloc(N);
    char* filename = (char*)malloc(FILENAME_SIZE);
    int count_of_lines = -1;
    read_argv(argc, argv, ServerPort_str, ServerAddr, filename); 

    check_null(ServerAddr);
    check_null(ServerPort_str);
    check_null(filename);
    
    int ServerPort = atoi(ServerPort_str);

    int sockfd;
    struct sockaddr_in addr;
 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    check_sockfd(sockfd);
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ServerPort);
    addr.sin_addr.s_addr = inet_addr(ServerAddr);

    int counter = 0, flag_no_connection = NO_CONNECTION;
    while(counter < 10)
    {
        if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) != 0) // no connection, try again, counter++
        {
            close(sockfd);
            counter ++;
            usleep(100*1000);
        }
        else
        {
            printf("Socket connected to server!\n");
            flag_no_connection = CONNECTED;
            break;
        }
    }
    check_connection(flag_no_connection);

    sent_get_msg(sockfd);

    read_msgs_from_server(sockfd, filename, ServerAddr, ServerPort);
    
    close(sockfd);
    return 0;
}

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void check_argc(int argc)
{
    if(argc!=4)
    {
        error("Invalid count of arguments. Enter port, address, file name and try again\n");
    }
}

void read_argv(int argc, char** argv,  char* port, char* adr, char* file_name)
{
     
    memset(port, 0x00, N);
    memset(adr, 0x00, N);
    memset(file_name, 0x00, FILENAME_SIZE);
    char* port_and_adr = (char*)malloc(sizeof(argv[1]));

    strcpy(port_and_adr, argv[1]);
    strcpy(file_name, argv[2]);

    if(strcmp(file_name, "get") == 0)
    {
        memset(file_name, 0x00, FILENAME_SIZE);
        strcpy(file_name, argv[3]);
    }
    
    unsigned int flag_dot = 0, j=0;
    for (int i = 0; i < strlen(port_and_adr); i++)
    {
        if(port_and_adr[i]!=':'&& flag_dot==0)
        {
            adr[i] = port_and_adr[i];
        }
        else if(port_and_adr[i]==':')flag_dot = 1;
        else if(flag_dot==1)
        {
            port[j] = port_and_adr[i];
            j++;
        }
    }
}

void check_null(char* ptr)
{
    if (ptr == NULL) 
    {
        error("Error in (char) array\n");
    }
}

void check_sockfd(int sockfd)
{
    if (sockfd < 0)
	{
		error("Can not create socket. Please try again.");
	}
}

void check_connection(int flag)
{
    if(flag==NO_CONNECTION)
    {
        error("Unfortunately, socket couldn't connect to the port");
    }
}

void sent_get_msg(int sockfd)
{
    const char get_msg[] = {'g', 'e', 't'};
    int n = write(sockfd, get_msg, sizeof(get_msg));
    if (n < 0)error("ERROR writing to socket");
}

void check_file(FILE* file)
{
    if(file == NULL)
    {
        error("Unable to open file for writting");
    }
}

int read_msgs_from_server(int sock, char* filename, char* addr, int port)
{
    char* tmp = (char*)malloc(MAX_SIZE);
    if(tmp)memset(tmp, 0x00, MAX_SIZE);
    
    FILE *file = fopen(filename, "w");
    check_file(file);

    int n = 0;
    while (n = read(sock, tmp, MAX_SIZE) > 0)
    {
        if(tmp[0] == '\n'){continue;}

        char* res = (char*)calloc(MAX_SIZE, 1);
        fullfill_str(tmp, res);
        fprintf(file, "%s:%d %s", addr, port, res); 
    }

    fclose(file);
}

void fullfill_str(char* buffer, char* res_str)
{
    int all_msg_len = 0;
    char phone[13] = { '\0' }, date[N] = { '\0' };
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
    strcpy(message, &buffer[all_msg_len]);

    int msg_num = ntohl(num_msg_32);
    transform_to_date(timestamp, date);
    int msg_len = ntohl(msg_len_32);

    sprintf(res_str, "%s %s %s", date, phone, message);
    //if (strcmp(message, "stop") == 0)flag_end = TRUE;
}

void transform_to_date(uint32_t bytes, char* date) {
    uint32_t unixTime = ntohl(bytes); 
    time_t timestamp = unixTime;
    struct tm* tm = localtime(&timestamp);
    tm->tm_isdst = -1;
    tm->tm_year += 1900;
    tm->tm_mon += 1;
    sprintf(date, "%02d.%02d.%d %02d:%02d:%02d", tm->tm_mday, tm->tm_mon, tm->tm_year,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
}
