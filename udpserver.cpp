#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <time.h>

#define BUFFER_SIZE 10000
#define TIMEOUT 30000  
#define MAX_MSGS 21
#define MAX_CLIENTS 101
#define FILENAME "msg.txt"
#define TIMEOUT_IN_POLL 0
#define ADDR_AND_PORT 30

typedef struct {
    struct sockaddr_in addr;
    int msg_numbers[MAX_MSGS];
    int msg_count;
    time_t last_activity;
    int needs_response; 
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int num_clients = 0;

void send_answer(ClientInfo client, int fd, struct sockaddr_in* addr, int addrlen);
void fullfill_str(char* buffer, char* res_str, int* num, int*len, int* flag_end);
void transform_to_date(uint32_t bytes, char* date);
void form_id_of_client(char* begin, struct sockaddr_in client_addr);
int check_poll(int p);
void configuration(int* listen_socks, int start, int num_ports, struct pollfd* pfds);
void check_bind(int b);
int set_non_block_mode(int s);
void check_socket(int s_desc);
void check_file(FILE* file);
void check_argc(int argc);
void error(const char *msg);

int find_client(struct sockaddr_in *addr) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            clients[i].addr.sin_port == addr->sin_port) {
            return i;
        }
    }
    return -1;
}

void remove_client(int index) {
    fprintf(stdout, "DELETE ! ! ! !\n");
    if (index >= 0 && index < num_clients) {
        for (int i = index; i < num_clients - 1; i++) {
            clients[i] = clients[i + 1];
        }
        num_clients--;
    }
}

void add_client(struct sockaddr_in *addr, int msg_number) {
    if (num_clients < FD_SETSIZE) {
        clients[num_clients].addr = *addr;
        for(int j = 0; j< MAX_MSGS;j++)clients[num_clients].msg_numbers[j] = -1;
        clients[num_clients].msg_count = 0; // was 1
        clients[num_clients].last_activity = time(NULL);
        clients[num_clients].needs_response = 1;
        num_clients++;
    }
}

int is_duplicate(int client_index, int msg_number) {
    ClientInfo *client = &clients[client_index];
    for (int i = 0; i < client->msg_count; i++) {
        if (client->msg_numbers[i] == msg_number) {
            fprintf(stdout, "DUBL %d\n", client->msg_numbers[i]);
            return 1;
        }
    }
    if (client->msg_count < MAX_MSGS) {
        client->msg_numbers[client->msg_count++] = msg_number;
    }
    client->last_activity = time(NULL);
    client->needs_response = 1;
    return 0;
}

void cleanup_clients() {
    time_t current_time = time(NULL);
    for (int i = 0; i < num_clients; i++) {
        if (difftime(current_time, clients[i].last_activity) > 30) {
            fprintf(stdout, "Client %d has been removed", i);
            remove_client(i);
            i--;
        }
    }
}

int main(int argc, char *argv[]) {
    check_argc(argc);

    int start_port = atoi(argv[1]);
    int end_port = atoi(argv[2]);
    int num_ports = end_port - start_port + 1;
    int listen_socks[num_ports];
    struct pollfd pfds[num_ports];

    FILE *logfile = fopen(FILENAME, "w");
    check_file(logfile);

    configuration(listen_socks, start_port, num_ports, pfds);

    int flag_end =0;
    
    while (flag_end!=1) 
    {
        int poll_count = poll(pfds, num_ports, TIMEOUT);
        if(check_poll(poll_count)==TIMEOUT_IN_POLL)continue;

        for (int i = 0; i < num_ports; i++) 
        {
            if (pfds[i].revents & POLLIN) 
            {
                char* buffer = (char*)calloc(BUFFER_SIZE, 1);

                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int r = recvfrom(pfds[i].fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);

                if (r > 0) 
                {
                    char begin[ADDR_AND_PORT] = {'\0'};
                    form_id_of_client(begin, client_addr);

                    char* res_str= (char*)calloc(BUFFER_SIZE, 1);
                    int msg_number = 0, msg_len = 0;
                    fullfill_str(buffer, res_str, &msg_number, &msg_len, &flag_end);

                    int client_index = find_client(&client_addr);
                    if (client_index == -1) {
                        add_client(&client_addr, msg_number);
                        client_index = num_clients - 1;
                    }

                    if (!is_duplicate(client_index, msg_number)) 
                    {
                        fprintf(logfile, "%s", begin);
                        fprintf(logfile, "%s\n", res_str);
                    }
                }
            }

            if (pfds[i].revents & POLLOUT) {
                for (int j = 0; j < num_clients; j++) {
                    if (clients[j].needs_response) {
                        fprintf(stdout, "Client index^ %d\n", j);
                        send_answer(clients[j], pfds[i].fd, &clients[j].addr, sizeof(clients[j].addr));
                        clients[j].needs_response = 0;  
                    }
                }
                if(flag_end==1){fprintf(stdout, "END FLAG\n");break;}
            }
        }
    }

    fclose(logfile);
    exit(EXIT_SUCCESS);
}

void form_id_of_client(char* begin, struct sockaddr_in client_addr)
{
    char client_id[INET_ADDRSTRLEN] = {'\0'}; 
    char client_port[INET_ADDRSTRLEN] = {'\0'};
    sprintf(client_id, "%s", inet_ntoa(client_addr.sin_addr));
    sprintf(client_port, "%d", ntohs(client_addr.sin_port));
    sprintf(begin, "%s:%s ", client_id, client_port);

}
void send_answer(ClientInfo client, int fd, struct sockaddr_in* addr, int addrlen){
    uint32_t copy[MAX_MSGS];
    char datagram[81] = {'\0'};
    int all_msg_len = 0;
    fprintf(stdout, "NUMS:");
    for(int i = 0; i< client.msg_count;i++)
    {
        if(client.msg_numbers[i]!=-1){copy[i] = htonl(client.msg_numbers[i]);
        fprintf(stdout, "%d, ", client.msg_numbers[i]);
        memcpy(&datagram[all_msg_len], &copy[i], 4);
        all_msg_len += 4;}
    };
    printf("SIZE OF DATA IS:  %d\n\n", all_msg_len);
    sendto(fd, datagram, all_msg_len,MSG_NOSIGNAL, (struct sockaddr*)addr, addrlen);
}

void fullfill_str(char* buffer, char* res_str, int* num, int*len, int* flag_end)
{
    int all_msg_len = 0;
    char phone[13] = { '\0' }, date[20] = { '\0' }, message[BUFFER_SIZE] = { '\0' };
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
    struct tm* tm = localtime(&timestamp);
    tm->tm_isdst = -1;
    tm->tm_year += 1900;
    tm->tm_mon += 1;
    sprintf(date, "%02d.%02d.%d %02d:%02d:%02d", tm->tm_mday, tm->tm_mon, tm->tm_year,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
}

int check_poll(int p)
{
    if (p < 0) 
    {
        error("Error in poll");
    }
    if (p == TIMEOUT_IN_POLL) 
    {
        cleanup_clients();
        return TIMEOUT_IN_POLL;
    }
}

void configuration(int* listen_socks, int start, int num_ports, struct pollfd* pfds)
{
    for (int i = 0; i < num_ports; i++) {
        listen_socks[i] = socket(AF_INET, SOCK_DGRAM, 0);
        check_socket(listen_socks[i]);

        set_non_block_mode(listen_socks[i]);

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(start + i);

        int b = bind(listen_socks[i], (struct sockaddr *)&server_addr, sizeof(server_addr));
        check_bind(b);

        pfds[i].fd = listen_socks[i];
        pfds[i].events = POLLIN | POLLOUT;
    }
}

void check_bind(int b)
{
    if (b < 0)error("Error in BIND");
}

int set_non_block_mode(int s)
{
    int fl = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, fl | O_NONBLOCK);
}

void check_socket(int s_desc)
{
    if (s_desc < 0)
	{
		error("Could not create socket. Please try again.");
	}
}

void check_file(FILE* file)
{
    if (file == NULL) 
    {
        error("Unable to open input file\n");
    }
}

void check_argc(int argc)
{
    if(argc!=3)
    {
        error("Invalid count of arguments. Enter port, address, file name and try again\n");
    }
}
void error(const char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(EXIT_FAILURE);
}