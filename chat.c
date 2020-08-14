/*
 * Author: 		Veronica Toriz
 *            Luis Tejeda
 *            Joshua Guizarnotegui
 * Description: Peer-to--Peer chat application in C
 *
 */
#include <netdb.h>
#include <ifaddrs.h>
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
#include <math.h>
#define MAX_CLIENTS	100

static unsigned int cli_count = 0, myPort=4534, peer_count=0;
static int uid = 1;
//static unsigned int terminate_id=0;
/* Client structure */
typedef struct {
	struct sockaddr_in addr;	/* Client remote address */
	int connfd;			/* Connection file descriptor */
	int uid;			/* Client unique identifier */
	char name[32];			/* Client name */
} client_t;

typedef struct {
	char ip[100];
	char port[100];
	int pid;
	int connfd;
} remote_peer;

client_t *clients[MAX_CLIENTS];
remote_peer * peers[MAX_CLIENTS];

/* Add client to queue */
void queue_add(client_t *cl){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(!clients[i]){
			clients[i] = cl;
			return;
		}
	}
}

/* Delete client from queue */
void queue_delete(int uid){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				return;
			}
		}
	}

}
/* Print ip address */
void print_client_addr(struct sockaddr_in addr){
	printf("%d.%d.%d.%d",
		addr.sin_addr.s_addr & 0xFF,
		(addr.sin_addr.s_addr & 0xFF00)>>8,
		(addr.sin_addr.s_addr & 0xFF0000)>>16,
		(addr.sin_addr.s_addr & 0xFF000000)>>24);
}

/* Delete client from queue */
void terminate_peer(int uid){
	int i;
	int deleted = 0;
	for(i=0;i<MAX_CLIENTS;i++){
		if(peers[i]){
			if(peers[i]->pid == uid){
			    char buff[1024] = "\\QUIT";
		        printf("\n[in terminate]Sending to SERVER: %s ",buff);

		        /* Send message to the server */
        		int in = write(peers[i]->connfd,buff,strlen(buff));
		        if (in < 0){
			     perror("\nClient Error: Writing to Server");
		         //return 0;
		        }
			    close(peers[i]->connfd);
	            free(peers[i]);
	            peer_count--;//delte peer from count

                /* Delete client from queue and yeild thread */
				peers[i] = NULL;
				deleted = 1;//set deleted to true
				printf("\n===========================\n");
        printf("   PEER ID:%d TERMINATED", uid);
        printf("\n===========================\n");
				return;
			}
		}
	}
	if (!deleted){
	    printf("\n ================================\n");
        printf(" ERROR! Invalid peer id? Try again");
        printf("\n ================================\n");
	}
}
/* Send message to all clients but the sender */
void send_message(char *s, int uid){
	int i;
	int ret;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid != uid){
				ret = write(clients[i]->connfd, s, strlen(s));
				//if (ret < 0)break;
				printf("write returns:%d\n", ret);
			}
		}
	}
}

/* Send message to all clients */
void send_message_all(char *s){
	int i, ret;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			ret = write(clients[i]->connfd, s, strlen(s));
			printf("write returns:%d\n", ret);
		}
	}
}

/* Send message to sender */
void send_message_self(const char *s, int connfd){
	int ret = write(connfd, s, strlen(s));
	printf("write returns:%d\n", ret);
}

/* Send message to client */
void send_message_client(char *s, int uid){
	int i, ret;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				ret =write(clients[i]->connfd, s, strlen(s));
				printf("write returns:%d\n", ret);
			}
		}
	}
}

/* Send list of active clients */
void send_active_clients(int connfd){
	int i;
	char s[64];
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			sprintf(s, "<<CLIENT %d | %s\r\n", clients[i]->uid, clients[i]->name);
			send_message_self(s, connfd);
		}
	}
}

/* Strip CRLF */
void strip_newline(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}

/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[4096];
	char buff_in[4096];
	int rlen;

	cli_count++;
	client_t *cli = (client_t *)arg; // casting from void * to client_t *

	printf("<<ACCEPT ID:%d", cli->uid);
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);

	sprintf(buff_out, "<<JOIN, HELLO %s\r\n", cli->name);
	send_message_all(buff_out);

    /* Receive input from client */
	while( (rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1)) > 0 ){

        buff_in[rlen] = '\0';
        buff_out[0] = '\0';
		strip_newline(buff_in);

		/* Ignore empty buffer */
		if(!strlen(buff_in)){
			continue;
		}
	    printf("\nCLIENTS COUNT:%d\n", cli_count);
		printf("***********************\nMessage received from ");
        print_client_addr(cli->addr);
        // This port is ntohs converted.
        printf("\nSender\'s port: %d\n", ntohs(cli->addr.sin_port));
        printf("Message:%s\n",buff_in);
        printf("***********************\n");

		/* Special options */
		if(buff_in[0] == '\\'){
			char *command;
			command = strtok(buff_in," ");
			if(!strcmp(command, "\\QUIT")){
				break;
			}else if(!strcmp(command, "\\ACTIVE")){
				sprintf(buff_out, "<<CLIENTS %d\r\n", cli_count);
				send_message_self(buff_out, cli->connfd);
				send_active_clients(cli->connfd);
			}else{
				send_message_self("<<UNKOWN COMMAND\r\n", cli->connfd);
			}
		}else{
			/* Send message */
			sprintf(buff_out, "[%s] %s\r\n", cli->name, buff_in);
			send_message(buff_out, cli->uid);
		}
	}

	/* \QUIT: Close connection */
	printf("\nWe received Close connection request.\n");
	close(cli->connfd);
	sprintf(buff_out, "<<LEAVE, BYE %s\r\n", cli->name);
	send_message_all(buff_out);

	/* Delete client from queue and yeild thread */
	queue_delete(cli->uid);
	printf("<<LEAVE ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());
	pthread_exit(NULL);
	return NULL;
}

/* Get power of x*/
int power(int x, int y){
    int p=1;
    for (int i=0; i<y; i++){
        p *= x;
    }
    return p;
}

/* ------------------------------
 *  Get System IP Address Method
 * ------------------------------*/
char * getIPAddress(){

    char * ipAddress=NULL;
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
    // retrieve the current interfaces - returns 0 on success
    success = getifaddrs(&interfaces);
    if (success == 0) {
        // Loop through linked list of interfaces
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if(temp_addr->ifa_addr->sa_family == AF_INET) {
                // Check if interface is en0 which is the wifi connection
                if(strcmp(temp_addr->ifa_name, "en0")){
                    ipAddress=inet_ntoa(((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);
    return ipAddress;
}

int get_size(char * a){
    int i=0;
    for (;a[i] !='\0'; i++ );
    return i+1;
}

/* List Peers */
void list_peers(){
	printf("No. of Peers:%d\n", peer_count);
	printf("\nid:  IP address\t\tPort No.\n");
    int j;
    char * tmp = malloc(10);
    char * ip_tmp = malloc(200);
    for(j=0;j<MAX_CLIENTS;j++){
        if (peer_count > 0){
            if(peers[j]){
            int i=0;
            for(; i<10; i++){
                if (peers[j]->port[i]==10){
                    tmp[i] = peers[j]->port[i];break;
                }
                tmp[i] = peers[j]->port[i];
            }
            tmp[i] = '\0';
            i=0;
            for(; i<200; i++){
                if (peers[j]->ip[i]==32){
                    ip_tmp[i] = peers[j]->ip[i];break;
                }
                ip_tmp[i] = peers[j]->ip[i];
            }
            ip_tmp[i] = '\0';

            //printf("i:%d\n", i);
        	printf(" %d:  %s\t%s\n", peers[j]->pid, ip_tmp, tmp);
            }
        }

    }
}

/* Connect */
void * connect_to_peer(char * ip, char * port){
	printf("\nConnecting to peer\n");
	//remote_peer *cli = (remote_peer *)arg; // casting from void * to remote_peer *
	remote_peer *cli = (remote_peer *)malloc(sizeof(remote_peer));

	strncpy(cli->ip,ip, strlen(ip));
	strncpy(cli->port,port, strlen(port));
	//printf("ip:%s, port:%s\n", cli->ip, cli->port);
	int fd = 0;
	char buff[1024];

	//Setup Buffer Array
	memset(buff, 0,sizeof(buff));

	//Create Socket
	fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd<0)
	{
		perror("Client Error: Socket not created succesfully");
		return NULL;
	}

	//Structure to store details
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
  int in, len;
	//Initialize
	server.sin_family = AF_INET;
	server.sin_port =  htons(atoi(cli->port));
  server.sin_addr.s_addr = inet_addr(cli->ip);

	//Connect to given server
	len = sizeof(server);
	in = connect(fd, (struct sockaddr *)&server, (socklen_t)len);
	if(in<0){
	    printf("ERROR Code:%d\n", in);
		perror("Client Error: Connection Failed.");
		return NULL;
	}else{
		peer_count++;
		cli->connfd = fd;
		cli->pid = peer_count;

		int i;
		//to keep track of the number of peers connected
		for(i=0;i<MAX_CLIENTS;i++){
			if(!peers[i]){
				peers[i] = cli;
				break;
			}
		}
		printf("\nSuccessfully connected to peer!\n");
	}

	return NULL;
}

/* Send to peer*/
void send_to_peer(int uid, char * msg){
	int i;
	int ret, found=0;
	for(i=0;i<MAX_CLIENTS;i++){
		if (peer_count > 0){
		    if(peers[i]){
			    if(peers[i]->pid == uid){
			        printf("ID:%d\nMessage to send:%s\n", uid, msg);
			        int bytes = get_size(msg);
	                printf("Sending to peer [bytes:%d] ................\n", bytes);
				    ret = write(peers[i]->connfd, msg, bytes);
				    if (ret < 0){
					    printf("Error in sending message to peer:%d\n", ret);
					    return;
				    }else{
				        found = 1;
					    break;
				    }
			    }
		    }
		}
		else{
		    break;
		}
	}
	if (!found){
	    printf(" ERROR! id not found? Try again");
	}
}


/* --------------------------------------
 *  Display Command Manual Thread Method
 * --------------------------------------*/
void * displayCommands(){
    char cmd[200];
    //int ret=0;
    char * ip = getIPAddress();
    while(1){
        memset(cmd, 0, sizeof(cmd));

        printf(">> ");
        char *p = fgets(cmd, 200, stdin);
        if (p == NULL){
            printf("Not valid command entered.\n");
        }else{
            char cmd2[200];
            strcpy(cmd2, cmd);
            char *s1=malloc(200);
            int i=0;
            int space1=-1,space2=-1;
            for (;i<strlen(cmd);i++){
                if (cmd[i] == 32){ // if SPACE found
                    //printf("SPACE Detected!\n");
                    if (space1==-1){ // if 1st space detected index is not saved already
                        space1=i;
                    }else{
                        space2=i;break;
                    }
                }
            }
            if (space2==-1){ // if not detected second space
                //printf("\nsecond space not detected.\n");
                space2 = strlen(cmd);
            }if (space1==-1){ // if not detected second space
                //printf("\nsecond space not detected.\n");
                space1 = 0;
            }
            //printf("\nSPACE1:%d, SPACE2:%d\n", space1,space2);
            //strncpy(s1, cmd+space1, -i); // i+1: index next to space
            s1 = strtok(cmd2, " ");

		      	//int l1=strlen(cmd);
		      	//int l2=strlen(s1);
		      	//printf("\ncmd:%d, substr:%d", l1, l2);
            //printf("\nYou entered: %ssubstr:%s\n------------------------\n",cmd, s1);
						if (strcmp(s1, "help\n") == 0){
							printf("\nAvailable Commands:\n");
							printf("_______________________________________________________________________________\n");
							printf(" 1. help: display the available commands\n");
							printf(" 1. myip: display the ip address\n");
							printf(" 2. connect <destination> <port no>: connect to another peer. \n");
							printf(" 3. send <connection id.> <message>: send messages to another peer \n");
							printf(" 4. myport: display the port number\n");
							printf(" 5. list: lists all the ip adresses and port numbers of the connected peers\n");
							printf(" 6. terminate <connection id.>: terminates the connection of the selected peer\n");
							printf(" 7. exit: exits the chat application\n");
							printf("_______________________________________________________________________________\n");
                //exit(1);
            }else if (strcmp(s1, "exit\n") == 0){
                printf("\n===========================\n");
                printf("     Good Bye!!!");
                printf("\n===========================\n");
                exit(1);
            }else if (strcmp(s1, "myip\n") == 0){
                printf("\n===========================\n");
                printf("   My IP:%s", ip);
                printf("\n===========================\n");
            }else if (strcmp(s1, "myport\n") == 0){
                printf("\n===========================\n");
                printf("   My PORT:%d", myPort);
                printf("\n===========================\n");
            }else if (strcmp(s1, "connect") == 0){
                if (space2 == strlen(cmd) ){
                    printf("Arguments Error: Usage connect <destination ip> <port no>\n");
                    continue;
                }
                printf("\n===========================\n");
                printf("   CONNECT <destination ip> <port no>");
                printf("\n===========================\n");

                char port[12];
                memset(s1, 0, 200);
                strncpy(s1, cmd+space1+1, space2-space1); // getting ip address from command
                strncpy(port, cmd+space2+1, strlen(cmd)-space2-1); // getting port no from command
                connect_to_peer(s1, port);

            }else if (strcmp(s1, "send") == 0){
                printf("\n=============================================\n");
                printf("    SEND <connection id.> <message>");
                printf("\n=============================================\n");
                if (peer_count == 0){
                    printf("No peer connected! Sending failed.\n");continue;
                }

                char msg[4096];
                memset(s1, 0, 200);

                strncpy(s1, cmd+space1+1, space2-space1); // getting connection id from command
                strncpy(msg, cmd+space2+1, strlen(cmd)-space2-1); // getting message from command

                send_to_peer(atoi(s1), msg);

            }else if (strcmp(s1, "list\n") == 0){
                printf("\n==================================================\n");
                list_peers();
                printf("\n==================================================\n");
            }else if (strcmp(s1, "terminate") == 0){
                int uid=0;
                if (space1 == 0){
                    printf("Arguments Error: Usage terminate <connection id.>\n");
                    continue;
                }
                memset(s1, 0, 200);
                strncpy(s1, cmd+space1, space2-space1); // i+1: index next to space
                int factor=0;
                for (int k=space2-space1-2; k>0; k--){
                    int no = s1[k];
                    int no2 = power(10, factor);
                    //printf("original no:%d, factor:%d\n",no-48, no2);
                    uid +=  (no-48) * no2;
                    factor++;
                }
                printf("uid:%d\n", uid);
                terminate_peer(uid);
            }
            //sleep(1);
        }
    }
    pthread_exit(NULL);
    return NULL;
}
/*
 * Sub Routine to accept incoming connections
 */
void * acceptConnections(){

  int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;
    // To get system IP
    char * myIP = getIPAddress();
    // My Port provided through command line
    //printf("\nIP:%s, myPORT:%d", myIP, myPort);

    /* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(myIP);//htonl(INADDR_ANY);
	serv_addr.sin_port = htons(myPort);

	/* Bind */
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Socket binding failed");
		exit(1);
		return NULL;
	}

	/* Listen */
	if(listen(listenfd, 10) < 0){
		perror("Socket listening failed");
		exit(1);
		return NULL;
	}

	printf("\n\n<[Accepting incoming connections]>\n");

    /* Accept clients */
	while(1){
	    //printf("\nI'm in connection loop back!\n");
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count+1) == MAX_CLIENTS){
			printf("<<MAX CLIENTS REACHED\n");
			printf("<<REJECT ");
			print_client_addr(cli_addr);
			printf("\n");
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->addr = cli_addr;
		cli->connfd = connfd;
		cli->uid = uid++;
		sprintf(cli->name, "%d", cli->uid);

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}
    pthread_exit(NULL);
    return NULL;
}


/* --------------------------------------------
 *          Main Chat Thread
 * --------------------------------------------
 */
int main(int argc, char *argv[])
{

	pthread_t user_tid, user_tid2;

    if(argc != 2){
		printf(" Usage: %s <port> \n",argv[0]);
		return 0;
	}
	myPort = atoi(argv[1]);
	//printf("PORT:%d", myPort);
	printf("\t==================================================\n");
	printf("\t  A Chat Application for Remote Message Exchange");
	printf("\n\t===================================================\n");

	  pthread_create(&user_tid2, NULL, &acceptConnections, NULL);
    pthread_create(&user_tid, NULL, &displayCommands, NULL);
    pthread_join(user_tid2, NULL);
    pthread_join(user_tid, NULL);

    printf("\nExiting Main thread.......\n\n");
}
