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

void error(const char *msg);
void check_argc(int argc);
void read_argv(int argc, char** argv,  char* port, char* adr, char* file_name);
void check_null(char*);
void check_sockfd(int sockfd);
void check_connection(int flag);
void sent_put_msg(int sockfd);
int read_file_to_list(char* filename);
void prepare_msg_for_sending(int count_of_msg);
void parse_string(char* string, char* date, char* time, char* phone, char* message);


struct Node {
    char data[MAX_SIZE];
    char msg_to_send[MAX_SIZE];
    int flag_ok;
    int msg_num;
    int size_msg_to_send;
    struct Node* next;
};

struct Node *head = NULL;
struct Node *current = NULL;

int main(int argc, char *argv[])
 {
    check_argc(argc);

    char* ServerPort_str = (char*)malloc(N);
    char* ServerAddr = (char*)malloc(N);
    char* filename = (char*)malloc(N);
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

    sent_put_msg(sockfd);

    int count_of_msg = read_file_to_list(filename), count_of_ok = 0;

    prepare_msg_for_sending(count_of_msg);
    
    current = head;

    while (count_of_msg!=count_of_ok) 
    {
        if(current->flag_ok==OK_NOT_RECIEVED)
        {
            int n = write(sockfd, current->msg_to_send, current->size_msg_to_send);
            if (n < 0)error("ERROR writing to socket");
            
            char response[2] = {'\0'};
            n = read(sockfd, response, sizeof(response));
            if (n < 0)error("ERROR reading from socket");
            else if (n==2)
            {
                if(response[0]=='o'&& response[1]=='k')
                {
                    current->flag_ok=OK_RECIEVED;
                    count_of_ok++;
                }    
            }
            else if(n==1)
            {
                if(response[0]=='o')
                {
                    while(response[0]!='k')n = read(sockfd, response, sizeof(response));
                    current->flag_ok=OK_RECIEVED;
                    count_of_ok++;
                }
            }
        }
        
        current = current->next;
        if(current==NULL)current=head;
    }
    
    close(sockfd);
    return 0;
}

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void check_argc(int argc)
{
    if(argc!=3)
    {
        error("Invalid count of arguments. Enter port, address, file name and try again\n");
    }
}
void read_argv(int argc, char** argv,  char* port, char* adr, char* file_name)
{
     
    memset(port, 0x00, sizeof(port));
    memset(adr, 0x00, sizeof(adr));
    memset(file_name, 0x00, sizeof(file_name));
    char* port_and_adr = (char*)malloc(sizeof(argv[1]));

    strcpy(port_and_adr, argv[1]);
    strcpy(file_name, argv[2]);
    
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
if (ptr == NULL) {
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

void sent_put_msg(int sockfd)
{
    const char put_msg[] = {'p', 'u', 't'};
    int n = write(sockfd, put_msg, sizeof(put_msg));
    if (n < 0)error("ERROR writing to socket");
}

int read_file_to_list(char* filename)
{
    char* tmp = (char*)malloc(MAX_SIZE);
    if(tmp)memset(tmp, 0x00, MAX_SIZE);
    
    FILE *infile = fopen(filename, "r");
    if (infile == NULL) {
        error("Unable to open input file");
    }
    int msg_counter = 0;
    while (fgets(tmp, MAX_SIZE, infile))
    {
        if(tmp[0] == '\n'){continue;}
        struct Node *newNode = (struct Node*)malloc(sizeof(struct Node));
        if (newNode == NULL)error("Error in creation new node.\n");
        
        strcpy(newNode->data, tmp);
        newNode->flag_ok=OK_NOT_RECIEVED;
        newNode->next = NULL;
        newNode->msg_num=msg_counter;
        msg_counter++;

        if (head == NULL) {
            head = newNode;
            current = newNode;
        } 
        else {
            current->next = newNode;
            current = newNode;
        }
    }

    fclose(infile);
    return msg_counter;
}

void prepare_msg_for_sending(int count_of_msg)
{
    current = head;
    while (current != NULL) {

        char date[N] = {"\0"}, time[N]={'\0'}, phone[N]={'\0'};
        char* message = (char*)malloc(MAX_SIZE-3*N);
        if(message)memset(message, 0x00, MAX_SIZE-3*N);
        parse_string(current->data, date, time, phone, message);
        uint32_t msg_num_network_order = htonl(current->msg_num);

        struct tm tm;
        strptime(date, "%d.%m.%Y %H:%M:%S", &tm);
        tm.tm_isdst = -1;
        time_t timestamp = mktime(&tm);
        timestamp = htonl(timestamp);
        uint32_t timestamp_network_order = timestamp; 

        uint32_t msg_len_network_order = htonl(strlen(message));
        
   
        char* msg = (char*)malloc(MAX_SIZE);
        memset(msg, 0x00, MAX_SIZE);
        int msg_len = 0;
        memcpy(&msg[msg_len], &msg_num_network_order, sizeof(msg_num_network_order));
        msg_len += sizeof(msg_num_network_order);
        memcpy(&msg[msg_len], &timestamp_network_order, sizeof(timestamp_network_order));
        msg_len += sizeof(timestamp_network_order);
        memcpy(&msg[msg_len], phone, 12);
        msg_len += 12;
        memcpy(&msg[msg_len], &msg_len_network_order, sizeof(msg_len_network_order));
        msg_len += sizeof(msg_len_network_order);
        memcpy(&msg[msg_len], message, strlen(message));
        msg_len += strlen(message);

        memcpy(current->msg_to_send, msg, msg_len);
        current->size_msg_to_send = msg_len;
        current = current->next;
    }
}

void parse_string(char* string, char* date, char* time, char* phone, char* message){
    char *p = strtok(string, " ");

    strcpy(date, p);
    p = strtok(NULL, " ");
    strcpy(time, p);

    p = strtok(NULL, " ");
    strcpy(phone,p);
    
    p = strtok(NULL, "");
    memcpy(message, p, strlen(p));

    strcat(date, " ");
    strcat(date, time);
}