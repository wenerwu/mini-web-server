/*
 *  A very simple demo of web server.
 *
 *  Author: Yunwen Wu
 *
 *  Date: 12.01.2017
 */ 


#include<stdlib.h>
#include<pthread.h>
#include<sys/socket.h>
#include<sys/types.h>       //pthread_t , pthread_attr_t and so on.
#include<stdio.h>
#include<netinet/in.h>      //structure sockaddr_in
#include<arpa/inet.h>       //Func : htonl; htons; ntohl; ntohs
#include<assert.h>          //Func :assert	
#include<string.h>          //Func :memset
#include<unistd.h>          //Func :close,write,read
#include <time.h>
#include<dirent.h>			//Func :opendir
#include <sys/stat.h>


#define PORT 5296
#define BACKLOG 10
#define MAX_BUFF 1024
#define MAX_CONN 100     //MAX connection limit
#define MAX_HASH 4096
#define HEADER 0xABAB
#define MAX_URL 100
#define MAX_BUFFF 999999

typedef struct user_s{
	char name[20];
	char password[20];
}user_t;

typedef struct ARG_s {
       int fd;
       sockaddr_in client_addr;
}ARG_t;

typedef struct client_s{
	int fd;
	int status;			//1 for connected; 0 for inconnected
	int port;
	char ip[17];
	int num;
}client_t;

typedef struct message_s{
	int header;
	int rtype;
	int length;
	char data[MAX_BUFF];
}message_t;

typedef struct package_s{
	int client_fd;
	int type;
	message_t info;
}package_t;

/*message:header, length,  type, data*/ 
/*header:0xABAB*/
enum packageType{RequestP, OrderP, ResponseP};
enum requestType{TimeR, NameR, ClientR, DisconnectR, MyportR};
enum orderType{MessageR};
enum fileType{HTML, TXT, JPG};

void* server_request(void* fd);
char* getTime_s(char* time);
int getClients(char* clients_info, int count);
int getMyport(char* client_info, int fd);
int isFolder(char* path);
int isDocument(char* path);
void send_obj(int fd, char* url);
void send_404(int fd);
int judgeType(char* url);
void send_post_response(int fd, int err, char* buff);
bool verify(char* url);
void init_user(void);

int packlen = sizeof(package_t);
client_t clientlist[100];
user_t user[10];
int usercount = 0;
int conn_count = 0;	//number of connections
int conn_Hash[MAX_HASH];
const char* filepath = "/home/wen/mywen/websocket/document";

int main(){
	int server_socket;
	int client_socket;
	
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	
	char buffer[MAX_BUFF];
	pthread_t requests[MAX_CONN];
	
	init_user();

	//create connection
	server_socket = socket(AF_INET, SOCK_STREAM, 0);	//ipv4, TCP
	if(server_socket == -1){
		printf("cannot create server!\n");
		return -1;
	}
	printf("create server success.\n");
	
	//initialize server
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;			//ipv4
	server_addr.sin_addr.s_addr = INADDR_ANY;	//anybody can connect
	server_addr.sin_port = htons(PORT);				
	
	if(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
		printf("cannot bind socket!\n");
		return -1;
	} 
	
	if(listen(server_socket, BACKLOG) == -1){
		printf("cannot listen to server!\n");
		return -1;
	}
	
	printf("initialize server success.\n");
	
	//accept reply from client
	int addrsize = sizeof(client_addr);
	while(1){
		printf("waiting for connection...\n");
		client_socket = accept(server_socket, (struct sockaddr*)&client_addr, (socklen_t *)&addrsize);
		if(client_socket == -1){
			printf("cannot connect to client!\n");
			break;
		}
		
		//record client
		printf("connect to client %d\n", client_socket);
		conn_Hash[client_socket] = conn_count;
		if(conn_count == MAX_CONN){
			printf("Reach the maximum connection!\n");
			continue;
		}
		
		clientlist[conn_count].fd = client_socket;
		clientlist[conn_count].port =  client_addr.sin_port;
		clientlist[conn_count].status = 1;
		strcpy(clientlist[conn_count].ip, inet_ntoa(client_addr.sin_addr));
		
		//establish response thread
		ARG_t arg;
		arg.fd = client_socket;
		arg.client_addr = client_addr;

		if(pthread_create(&requests[conn_count], NULL, server_request, (void*)&arg) == -1){
			printf("cannot create response!\n");
			break;
		}
		
	
		conn_count++;
		

	}
	
	
	//destroy connection
	close(server_socket);
	for(int i = 0; i < conn_count; i++){
		pthread_join(requests[i], NULL);
	}

} 

/* Answer client's request */
void* server_request(void* client_socket){
	ARG_t arg = *(ARG_t*)client_socket;
	int fd = arg.fd;
	struct sockaddr_in client_addr = arg.client_addr;
	
	int close_flag = 0;

	char *buff = (char*)malloc(packlen);
	char *entirebuff = (char*)malloc(3 * packlen);
	
	char* clients_info = (char*)malloc(3 * MAX_BUFF * sizeof(char));
	
	char* client_me = (char*)malloc(sizeof(client_t));
	char hostname[32];
	
	int length;
	char client_mesg[64];
	int another_num;
	char templine[MAX_BUFF];
	int flag = 0;
	char temp;
	char  old;
	int exist = 0;
		
	
	memset(entirebuff, 0, sizeof(entirebuff));
	
	while(!flag){
		memset(buff, 0, sizeof(buff));
		old = 0;
	
		int temp = recv(fd, buff, packlen, 0);
		if(temp == -1){
			printf("Receive from %d error!\n", fd);
			return NULL;
		}else if(temp == 0){
			printf("closing client socket %d.\n", fd);
			close_flag = 1;
			return NULL;
		}
	
		printf("Receiving message from %s\n", inet_ntoa(client_addr.sin_addr));
		
		printf("\n%s\n", buff);
		
		for(int i = 1; i < packlen; i++){
			if(buff[i] == '\n'){
				if(buff[i - 1] == '\r' && buff[i - 2] == '\n' && buff[i - 3] == '\r'){		//  /r/n/r/n
					flag = 1;
					break;
				}
			}
		}
		
		strcat(entirebuff, buff);
	}
	char method[7];
	char* url = (char*)malloc(MAX_URL);
	char protocol[100];
	char tempurl[100];
	
	
	memset(method,0,sizeof(method));
	sscanf(entirebuff, "%s%s%s", method, tempurl, protocol);
	strcpy(url, filepath);
	strcat(url, tempurl); 
		
	printf("\n\n m:%s\n url:%s\n p:protocol:%s\n\n", method, url, protocol);
	
	
	if(strcmp(method, "GET") == 0){
		if(isFolder(url) || isDocument(url)){
			exist = 1;
			if(isFolder(url)){
				printf("%s is folder\n", url);
				strcat(url, "index.html");
				if(!isDocument(url)){
					printf("index does not exist!%s\n", url);
					exist = 0;
				}
					
				else
					send_obj(fd, url);
			}
			else{
				printf("%s is document\n", url);
				send_obj(fd, url);		
			}
		}
		else{
			printf("No such filepath:%s\n", url);
			
		}
		
		if(!exist)
			send_404(fd); 
	}
	else if(strcmp(method, "POST") == 0){
		if(strstr(url, "dopost") == NULL){
			exist = 0;
			send_post_response(fd, 1, entirebuff);
		}
		else{
			send_post_response(fd, 0, entirebuff);
		}
	}
	else {
		printf("Invalid method:%s\n", method);
	}

	close(fd);

	//update client list
	if(conn_Hash[fd] != conn_count - 1){			//not last one
		clientlist[conn_Hash[fd]] = clientlist[conn_count - 1];
	}
	conn_Hash[clientlist[conn_Hash[fd]].fd] = conn_Hash[fd];
	conn_count --;
	printf("Client socket %d disconnected.\n",fd);
	

}

/* Get current time, and change it into gmt */
char* getTime_s(char* buf){
	time_t t;  
	tm* local;   
	tm* gmt;   
	
	t = time(NULL); 
	local = localtime(&t); 
	strftime(buf, 64, "%Y-%m-%d %H:%M:%S", local); 
	
	return buf;
}

/* Copy client info */
int getClients(char* clients_info, int conn_count){
	int num = 0;

	client_t tempclient[MAX_CONN];
	 
	for(num = 0; num < conn_count; num++){
		tempclient[num] = clientlist[num];
		tempclient[num].num = num;
	}


	memcpy(clients_info, &conn_count, sizeof(int));
	memcpy(clients_info + sizeof(conn_count) + 1, tempclient, conn_count * sizeof(client_t));	


}

int getMyport(char* client_info, int fd){
	int num = conn_Hash[fd];
	
	memcpy(client_info, &clientlist[num], sizeof(client_t));
}

int isFolder(char* path){

    if (opendir(path) == NULL)
    {
        return 0;
    }

    return 1;	
}

int isDocument(char* path){
	if(access(path, F_OK) == 0)
		return 1;
	return 0;
}

/* Send object to client */
void send_obj(int fd, char* url){
	char buff[MAX_BUFFF];
	char time_local[128];
	memset(buff, 0, packlen);
	memset(time_local, 0, sizeof(time_local));
	
	struct stat statbuf; 
  	stat(url, &statbuf); 


	char contenttype[100];
	int type = judgeType(url);
	switch (type){
		case HTML:
			strcpy(contenttype, "text/html");
			break;
			
		case TXT:
			strcpy(contenttype, "text/plain");
			break;
		
		case JPG:
			strcpy(contenttype, "image/jpeg");
			break;
			
		default:
			strcpy(contenttype, "text");
			break;
	} 
	
	char firstline[100] = "HTTP/1.1 200 OK\r\n";
	char secondline[100] =  "Content-Type: ";  //
	char thirdline[100] = "Content-Length: ";
	
	//data
	FILE *fp = fopen(url, "rb"); 
	char ch;
	char chh[2];
	chh[1] = 0;
	
	fseek(fp,0L,SEEK_END); 
	int size = ftell(fp);
	sprintf(buff, "%s%s%s\r\n%s%d\r\n\r\n", firstline, secondline, contenttype, thirdline, size);
		
	
	rewind(fp);
	int count = strlen(buff);
	char* data_buff = (char*)malloc((size + 1) * sizeof(char));
	memset(data_buff, 0, size + 1);
	
	int readb;
	readb =	fread(data_buff, sizeof(char), size, fp);

	memcpy(buff + count, data_buff, size); 
	
	send(fd, buff, count + size + 1, 0);
	

	
	fclose(fp);
	
}

void send_404(int fd){
	char buff[MAX_BUFF];
	char firstline[100] = "HTTP/1.1 404 BAD\r\n";
	char secondline[100] =  "Content-Type: ";  //
	char thirdline[100] = "Content-Length: ";

	sprintf(buff, "%s%s%s\r\n%s%d\r\n\r\n", firstline, secondline, "text/plain", thirdline, 0);
	
	send(fd, buff, strlen(buff), 0);
}

int judgeType(char* url){
	char* pos = strchr(url, '.');

	pos++;
	if(strcmp(pos, "html") == 0){
		return HTML;
	}
	if(strcmp(pos, "txt") == 0){
		return TXT;
	}
	if(strcmp(pos, "jpg") == 0){
		return JPG;
	}
	printf("error:%s\n", pos);
	return -1;
	
}

/* Respond to client */
void send_post_response(int fd, int err, char* buff){
	
	char firstlinetrue[100] = "HTTP/1.1 200 OK\r\n";
	char firstlinefalse[100] = "HTTP/1.1 404 BAD\r\n";
	char secondline[100] =  "Content-Type: ";  
	char thirdline[100] = "Content-Length: ";
	char response_info[100];
	memset(response_info, 0, 100);
	
	if(err){
		sprintf(buff, "%s%s%s\r\n%s%d\r\n\r\n", firstlinefalse, secondline, "text/plain", thirdline, 0);
		return;
	}
		
		
	bool iscorrect = verify(buff);
	
	if(iscorrect)
		strcpy(response_info, "<html><body>Login success!</body></html>");
	else
		strcpy(response_info, "<html><body>Login failure!</body></html>");
	
	int size = strlen(response_info);
	

	sprintf(buff, "%s%s%s\r\n%s%d\r\n\r\n%s", firstlinetrue, secondline, "text/html", thirdline, size, response_info);
	
	send(fd, buff, strlen(buff), 0);
}

/* Judge login info is correct */
bool verify(char* buff){
	char* info = strstr(buff, "\r\n\r\n");
	char name[20];
	char password[20];
	int count = 0;
	char ch;
	
	if(info == NULL){
		printf("Invalid info!\n");
		return false;
	}	
	info += 4;
	info = strstr(info, "login=");
	info += 6;
	
	while((ch = *(info + count)) != '&'){
		name[count++] = ch;
	}
	name[count] = 0;
	
	info = strstr(info, "pass=");
	info += 5;
	
	strcpy(password, info);

	for(int i = 0; i < usercount; i++){
		printf("%s %s %s %s  %d\n", name, password, user[0].name, user[0].password,strcmp(name, user[i].name));
		if((strcmp(name, user[i].name) == 0) && (strcmp(password, user[i].password) == 0))
			return true;
	}
	
	return false;
	
}

void init_user(void){
	strcpy(user[0].name, "3150105296");
	strcpy(user[0].password, "5296");

	usercount = 1;
}


