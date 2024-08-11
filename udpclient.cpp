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

#define MAX_ATTEMPTS 10
#define N 20
#define WAIT_TIME_MS 100
#define MAX_SIZE 350824
#define NO_CONNECTION 0
#define CONNECTED 1
#define RECIEVED 1
#define NOT_RECIEVED 0
#define MSG_LIMIT 20
#define SIZEOF_DATAGRAM 20*4*100
#define NUM_IN_LIST 12
#define NUM_NOT_IN_LIST 13

int init();
void deinit();
int sock_err(const char* function, int s);
void check_argc(int argc);
void error(const char* msg);
void read_argv(int argc, char** argv, char* port, char* adr, char* file_name);
void check_null(char* ptr);
void check_sockfd(int sockfd);
void check_file(FILE* infile);
int check_if_num_in_list(int num, int* nums, int len);
int read_file_to_list(char* filename);
void prepare_msgs_for_sending(int count_of_msg);
void parse_string(char* string, char* date, char* time, char* phone, char* message);

struct Node {
	char data[MAX_SIZE];
	char msg_to_send[MAX_SIZE];
	int recieved; 
	int msg_num;
	int size_msg_to_send;
	struct Node* next;
};

struct Node* head = NULL;
struct Node* current = NULL;


int main(int argc, char** argv)
{
	check_argc(argc);
	char* ServerPort_str = (char*)malloc(N);
	char* ServerAddr = (char*)malloc(N);
	char* filename = (char*)malloc(N);
	int count_of_lines = -1;

	read_argv(argc, argv, ServerPort_str, ServerAddr, filename);
	int ServerPort = 0;
	if(ServerPort_str)ServerPort = atoi(ServerPort_str);
	
	SOCKET sockfd;
	struct sockaddr_in addr;
	init();
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); // вместо IPPROTO_UDP было 0
	check_sockfd(sockfd);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(ServerPort);
	if(ServerAddr)addr.sin_addr.s_addr = inet_addr(ServerAddr);

	int count_of_msg = read_file_to_list(filename);
	int count_of_recieved = 0, recv_nums[MSG_LIMIT + 1] = { 0 };
	prepare_msgs_for_sending(count_of_msg);

	current = head;
	int lim = MSG_LIMIT;
	if (count_of_msg < MSG_LIMIT)lim = count_of_msg;

	while (count_of_recieved != lim)
	{
		if (current->recieved == NOT_RECIEVED)
		{
			int n = sendto(sockfd, current->msg_to_send, current->size_msg_to_send, 0, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
			if (n <= 0)sock_err("sendto", sockfd);

			char datagram[SIZEOF_DATAGRAM] = { '\0' };
			struct timeval tv = { 0, 100 * 1000 };
			fd_set fds;
			FD_ZERO(&fds); FD_SET(sockfd, &fds);
			int sizeof_addr = sizeof(struct sockaddr_in);

			n = select(sockfd + 1, &fds, 0, 0, &tv);
			if (n > 0)
			{
				int received = recvfrom(sockfd, datagram, sizeof(datagram), 0, (struct sockaddr*)&addr, &sizeof_addr);
				int cur_count_of_recv = received / sizeof(int32_t);

				for (int i = 0; i < cur_count_of_recv; i++) {
					int32_t msg_num_net;
					memcpy(&msg_num_net, &datagram[i * sizeof(int32_t)], sizeof(int32_t));
					int recv_msg_num = ntohl(msg_num_net);
					if (check_if_num_in_list(recv_msg_num, recv_nums, count_of_recieved) == NUM_NOT_IN_LIST)
					{
						if (current->msg_num == recv_msg_num)current->recieved = RECIEVED; /// new
						recv_nums[count_of_recieved] = recv_msg_num;
						count_of_recieved++;
					}

				}
			}
		}
		current = current->next;
		if (current == NULL)
		{
			current = head;
		}
	}

	closesocket(sockfd);
	deinit();
	return 0;
}

void prepare_msgs_for_sending(int count_of_msg)
{
	current = head;
	while (current != NULL) {

		char date[N] = { "\0" }, time[N] = { '\0' }, phone[N] = { '\0' };
		char* message = (char*)malloc(MAX_SIZE - 3 * N);
		if (message)memset(message, 0x00, MAX_SIZE - 3 * N);
		parse_string(current->data, date, time, phone, message);
		uint32_t msg_num_network_order = htonl(current->msg_num);

		struct tm tm;
		if (sscanf(date, "%d.%d.%d %d:%d:%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year, &tm.tm_hour, &tm.tm_min, &tm.tm_sec));
		tm.tm_isdst = -1;
		tm.tm_year -= 1900; 
		tm.tm_mon--;
		time_t timestamp = mktime(&tm);
		timestamp = htonl(timestamp);
		uint32_t timestamp_network_order = timestamp;

		uint32_t msg_len_network_order = 0;
		if(message) msg_len_network_order = htonl(strlen(message));


		char* msg = (char*)malloc(MAX_SIZE); // for sending 
		int msg_len = 0;
		if (msg)
		{
			memset(msg, 0x00, MAX_SIZE);
			memcpy(&msg[msg_len], &msg_num_network_order, sizeof(msg_num_network_order));
			msg_len += sizeof(msg_num_network_order);
			memcpy(&msg[msg_len], &timestamp_network_order, sizeof(timestamp_network_order));
			msg_len += sizeof(timestamp_network_order);
			memcpy(&msg[msg_len], phone, 12);
			msg_len += 12;
			memcpy(&msg[msg_len], &msg_len_network_order, sizeof(msg_len_network_order));
			msg_len += sizeof(msg_len_network_order);
			if(message)memcpy(&msg[msg_len], message, strlen(message));
			msg_len += strlen(message);

			memcpy(current->msg_to_send, msg, msg_len);
		}
		current->size_msg_to_send = msg_len;
		current = current->next;
	}
}

void parse_string(char* string, char* date, char* time, char* phone, char* message) 
{
	char* ptr = strtok(string, " ");

	strcpy(date, ptr);
	ptr = strtok(NULL, " ");
	strcpy(time, ptr);

	ptr = strtok(NULL, " ");
	strcpy(phone, ptr);

	ptr = strtok(NULL, "");
	memcpy(message, ptr, strlen(ptr));

	strcat(date, " ");
	strcat(date, time);
}

int check_if_num_in_list(int num, int* nums, int len)
{
	for (int i = 0; i < len; i++)
	{
		if (nums[i] == num)
			return NUM_IN_LIST;
	}
	return NUM_NOT_IN_LIST;
}

int read_file_to_list(char* filename) // return: количество сообщений в файле
{
	char* tmp = (char*)malloc(MAX_SIZE);
	if (tmp)memset(tmp, 0x00, MAX_SIZE);

	FILE* infile = fopen(filename, "r");
	check_file(infile); 

	int msg_counter = 0;
	while (fgets(tmp, MAX_SIZE, infile))
	{
		if (tmp[0] == '\n') { continue; }

		struct Node* new_node = (struct Node*)malloc(sizeof(struct Node));
		if (new_node == NULL)error("Error in creation new node.\n");

		if (new_node) 
		{
			strcpy(new_node->data, tmp);
			new_node->recieved = NOT_RECIEVED;
			new_node->next = NULL;
			new_node->msg_num = msg_counter;
		}
		msg_counter++;

		if (head == NULL) 
		{
			head = new_node;
			current = new_node;
		}
		else 
		{
			if(new_node)current->next = new_node;
			current = new_node;
		}
	}

	fclose(infile);
	return msg_counter;
}

void check_file(FILE* infile)
{
	if (infile == NULL) 
	{
		error("Unable to open input file\n");
	}
}

void check_sockfd(int sockfd)
{
	if (sockfd < 0)
	{
		error("Can not create socket. Please try again.");
	}
}

void read_argv(int argc, char** argv, char* port, char* adr, char* file_name)
{

	memset(port, 0x00, N);
	memset(adr, 0x00, N);
	memset(file_name, 0x00, N);
	char* port_and_adr = (char*)malloc(sizeof(argv[1]));
	
	if (port_and_adr)
	{ 
		memset(port_and_adr, 0x00, sizeof(argv[1]));
		strcpy(port_and_adr, argv[1]);
		strcpy(file_name, argv[2]);

		unsigned int flag_dot = 0, j = 0;
		for (int i = 0; i < strlen(port_and_adr); i++)
		{
			if (port_and_adr[i] != ':' && flag_dot == 0)
			{
				adr[i] = port_and_adr[i];
			}
			else if (port_and_adr[i] == ':')flag_dot = 1;
			else if (flag_dot == 1)
			{
				port[j] = port_and_adr[i];
				j++;
			}
		}
		check_null(port);
		check_null(adr);
		check_null(file_name);
	}
}

void error(const char* msg)
{
	printf("%s", msg);
	exit(EXIT_FAILURE);
}

void check_argc(int argc)
{
	if (argc != 3)
	{
		error("Invalid count of arguments. Enter port, address, file name and try again\n");
	}
}

int init()
{
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
}

void deinit()
{
	WSACleanup();
}

int sock_err(const char* function, int s)
{
	int err;
	err = WSAGetLastError();
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

void check_null(char* ptr)
{
	if (ptr == NULL) 
	{
		error("Error in (char) array\n");
	}
}



