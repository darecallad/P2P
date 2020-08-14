
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <time.h>
#include <math.h>


#define INTBUFF 1024//size for stdin(ints)
#define STRNGBUFF 100//size for IP, messages, tokens (stringd)
#define MAX_CON_QUE 4//# of connections that can be queued
#define FDSTDIN 0 //file descriptor for stdin
#define MAXMISSES 3
#define MAXARGC 4
#define INF 30

int sendToAll(int srvCount, int fromstep, int srvid,  int* cfds,
	int* lcost, int ctable[][srvCount])
{

	char sndBuff[INTBUFF];
	char outBuff[STRNGBUFF];
	memset(sndBuff, 0, sizeof(sndBuff));
	memset(outBuff, 0, sizeof(outBuff));
	sprintf(outBuff, "%d %d", srvCount, srvid);
	strcat(sndBuff, outBuff);

	for( int destcostid = 0; destcostid < srvCount; destcostid++)
	{
			memset(outBuff, 0, sizeof(outBuff));
			sprintf(outBuff, " %d %d", destcostid, ctable[srvid][destcostid]);
			strcat(sndBuff, outBuff);
	}
	for(int neighbid = 0; neighbid < srvCount; neighbid++)
	{
			if(cfds[neighbid] == 0)
			continue;
			int bytessent = send(cfds[neighbid], sndBuff, strlen(sndBuff), 0);
			//check if send failed
			if( bytessent < 0)
			{
					if(fromstep == 1)
					{
							fprintf(stderr, "\nstep: Routing Update Send to Server %d Failed.\n\n",
							neighbid+1);
							return -1;
					}
					else
					{
						  fprintf(stderr, "\nError: Routing Update Send to Server %d Failed.\n\n",
							neighbid+1);
						  continue;
					}
		  }
		  else
			{

				fprintf(stderr, "\nRouting Update Packet Sent to Server:  %d.\n\n", neighbid+1);
			}

	}
	return 0;

}


int disSendToNeighbor(int srvCount, int srvid, int dstid, int* cfds,
	int* lcost, int ctable[][srvCount])
{
	  double inf = INFINITY;

		if((dstid< 0) || (dstid >=srvCount))
		{
			fprintf(stderr, "\ndisable: ID  %d is not valid.\n\n", dstid+1);
			return -1;
		}

	  if(cfds[dstid] == 0)
	  {
		  fprintf(stderr, "\ndisable: Server %d is not a neighbor.\n\n", dstid+1);
		  return -1;
	  }
		char sndBuff[INTBUFF];
		char outBuff[STRNGBUFF];
		memset(sndBuff, 0, sizeof(sndBuff));
		memset(outBuff, 0, sizeof(outBuff));
		sprintf(outBuff, "%d %d", 1, srvid);
		strcat(sndBuff, outBuff);

		int bytessent = send(cfds[dstid], sndBuff, strlen(sndBuff), 0);
		if( bytessent < 0)
		{
			fprintf(stderr, "\ndisable: Link disable packet send to Server %d failed.\n\n", dstid+1);
			return -1;
		}
		else
		{

			fprintf(stderr, "\nLink disable packet sent to Server %d.\n\n", dstid+1);
		}
		lcost[dstid] = inf;
		ctable[srvid][dstid] = inf;
		ctable[dstid][srvid] = inf;
		cfds[dstid] = 0;
		return 0;



}
int upSendToNeighbor(int srvCount, int srvid, int dstid, int upcost, int* cfds,
	int* lcost, int ctable[][srvCount])
{

	if((dstid< 0) || (dstid >=srvCount))
	{
			fprintf(stderr, "\nupdate: Id %d is not valid.\n\n", dstid+1);
			return -1;
	}

	if(cfds[dstid] ==0)
	{
			fprintf(stderr, "\nupdate: Server %d is not a neighbor of this server.\n\n", dstid+1);
			return -1;
	}
	char sndBuff[INTBUFF];
	char outBuff[STRNGBUFF];
	memset(sndBuff, 0, sizeof(sndBuff));
	memset(outBuff, 0, sizeof(outBuff));
	sprintf(outBuff, "%d %d", 1, srvid);
	strcat(sndBuff, outBuff);
	memset(outBuff, 0, sizeof(outBuff));
	sprintf(outBuff, "%d %d", dstid, upcost);
	strcat(sndBuff, outBuff);
	int bytessent = send(cfds[dstid], sndBuff, strlen(sndBuff), 0);
	if( bytessent < 0)
	{
			fprintf(stderr, "\nupdate: Link update packet send to Server %d failed.\n\n", dstid+1);
			return -1;
	}
	else
	{

			fprintf(stderr, "\nLink update packet sent to Server %d.\n\n", dstid+1);
	}
	lcost[dstid] = upcost;
	ctable[srvid][dstid] = upcost;
	ctable[dstid][srvid] = upcost;

	return 0;

}
int sendhandler(int scount, int toall, int fromstep, int sid, int did, int term, int upcost, int* cfds,
	int* lcost, int ctable[][scount])
{
	//implement send to all function
	//Send coming from periodic update or step, send cost vector to all neighbors, numfields = scount
	if(toall == 1)
	{


		sendToAll(scount, 0, sid, cfds, lcost,ctable);
	}





	//Send coming from disable, Send a disable packet to specified neighbor, numfields = 0
	else if( term == 1)
	{

    disSendToNeighbor(scount, sid, did,  cfds, lcost, ctable);

	}

	//Send coming from update, Send link update packet to specified neighbor, numfields = 1 or -1
	else
	{


		upSendToNeighbor(scount,  sid,  did,  upcost,  cfds,
        lcost,  ctable);

	}
}

int fdSelect(int sfd, int* cfds, int*rfds, int snum, fd_set *fdset)
{
	int max = -1; //Update after every insert to fdset to find max fd
	FD_ZERO(fdset);	//Clear fdset
	FD_SET(FDSTDIN, fdset); //Insert stdin to fdset
	if(max < FDSTDIN)
		max = FDSTDIN;
	FD_SET(sfd, fdset); //Insert server's fd to fdset
	if(max < sfd)
		max = sfd;
	//Insert any active client fds to fdset, probably will never be set since these sockets
	// only handle sends unless a crash command is sent and all links are //closed by a crahsing server
	for( int i=0; i<snum; i++)
	{
		if(cfds[i] <= 0)
			continue;
		FD_SET(cfds[i], fdset);
		if(max < cfds[i])
			max = cfds[i];
	}

	//Insert any active receive fds to fdset
	for( int j=0; j<snum; j++)
	{
		if(rfds[j] <= 0)
			continue;
		FD_SET(rfds[j], fdset);
		if(max < rfds[j])
			max = rfds[j];
	}
	return max;
}
int main(int argc, char **argv)
{

	printf("\n\t==================================================================\n");
	printf("\t\t\t  DISTANCE VECTOR ROUTING PROTOCOL  \n");
	printf("\n\t==================================================================\n\n");


	//validate entry arguments
	if (argc != 5)
	{
		perror("\nError: Incorrect number of arguments.\n"
			"Usage: ./server -t <topology-file-name> -i <routing-update-interval>\n");
		return -1;
	}

	FILE *fp = fopen( argv[2],"r");
 	if (fp == NULL)
	{
		fprintf(stderr, "Error: Failed to open filename: %s \n", argv[2]);
		//	 ./server -t <topology-file-name> -i <routing-update-interval>
		return -1;
	}
	int updateInterval = atoi(argv[4]); //convert string to int
	if (updateInterval <= 0)
	{
		fprintf(stderr,"\nError: Invalid routing update interval.\n"
			"\tUpdate interval should be a positive digit>\n");
		fclose(fp);//close the file
		return -1;
	}

  //gets number of servers
	char readBuf[INTBUFF];
	memset(readBuf, 0, sizeof(readBuf));//returns a pointer to readBuf
	//reads a line from the specified fp and stores it into readBuf
	//stops when INTBUFF-1 are read
	fgets(readBuf, INTBUFF, fp);
	int numOfServers = atoi(readBuf);

	//Initialize routing table and set it's entries to inf

	int costTable[numOfServers][numOfServers];
	int linkCost[numOfServers];
	for( int i = 0; i < numOfServers; i++)
	{
		linkCost[i] = INF;
		for( int j = 0; j<numOfServers; j++)
		{
			costTable[i][j] = INF;
		}
	}

	//Get the number of neighbors from file
	//returns a pointer to readBuf
	memset(readBuf, 0, sizeof(readBuf));
	fgets(readBuf, INTBUFF, fp);
	//atoi(readBuf)converts the string argument into to an integer
	//int neibcount = atoi(readbuf);
	int numOfNeighbrs = atoi(readBuf);

	//Get and store ips and ports by network id, topology ids must be serial for this to work
	//Assumes first ip/port combo listed in topology file is this server's ip and port
	char servIPs[numOfServers][STRNGBUFF];
	memset(servIPs, 0, sizeof(servIPs));
	int servPorts[numOfServers];
	int serverID=0;
	for( int i = 0; i < numOfServers; i++)
	{
		memset(readBuf, 0, sizeof(readBuf));
		fgets(readBuf, INTBUFF, fp);
		char* arg;
		arg = strtok(readBuf, " \n"); //Tokenize buffer
		int actSerId = atoi(arg)-1;
		//int workingid = atoi(arg)-1;
		fprintf(stderr,"serverID value: %d\n",  serverID);
		if( i == 0)
		   serverID = actSerId;
		arg = strtok(NULL, " \n");
		strcpy(servIPs[actSerId], arg);
		arg = strtok(NULL, " \n");
		servPorts[actSerId] = atoi(arg);
		fprintf(stderr,"working id: %d\n",  actSerId);

	}

	//Update cost to server's neighbors and itself and set neighbor flags
	int neigLinks[numOfServers];
	memset(neigLinks, 0, sizeof(neigLinks));
	int serMissed[numOfServers];
	memset(serMissed, 0, sizeof(serMissed));
	for( int i = 0; i <  numOfNeighbrs; i++)
	{
		memset(readBuf, 0, sizeof(readBuf));
		fgets(readBuf, INTBUFF, fp);
		char* arg;
		arg = strtok(readBuf, " \n"); //Tokenize buffer
		int server_id = atoi(arg) - 1;
		arg = strtok(NULL, " \n");
		int neighbor = atoi(arg) - 1;
		arg = strtok(NULL, " \n");
		int cost = atoi(arg);
		neigLinks[neighbor] = 1;
		costTable[server_id][neighbor] = cost;


		costTable[neighbor][server_id] = cost;//cost is bidirectional
		linkCost[neighbor] = cost;

	}
	costTable[serverID][serverID] = 0;
	linkCost[serverID] = 0;
	fclose(fp);//closes the file
	printf("\n\nEnd of the reading file.\n" );




int clientfds[numOfServers];
memset(clientfds, 0, sizeof(clientfds));
int recvfds[numOfServers];
memset(recvfds, 0, sizeof(recvfds));
struct sockaddr_in server_addr;
//Iitial connection
//create socket
int serverfd = socket(AF_INET, SOCK_STREAM, 0);
if(serverfd < 0)
{
	fprintf(stderr, "Error: Cannot create server socket.\n");
	return -1;
}
bzero(&server_addr, sizeof(server_addr)); //Empty server_addr
//Fill server_addr with server's information
server_addr.sin_family = AF_INET; //Use IvP4
inet_pton(AF_INET, servIPs[serverID], &server_addr.sin_addr);//sets IP adrrs
server_addr.sin_port = htons(servPorts[serverID]);//sets port number

//Attempt to bind
if(bind(serverfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
{
	fprintf(stderr, "Error: Bind failed.\n");
	return -1;
}
//Attempt to listen
if(listen(serverfd, numOfServers) < 0)
{
	fprintf(stderr, "Error: Unable to listen on port %d.\n", servPorts[serverID] );
	return -1;
}
//mesage if the server connection was succesful
fprintf(stderr, "Server Connection succesful!\n Server's ID: %d\n\n", serverID + 1 );



  //connect to the other Servers
	int neibConnected = 0;
//	int workingid = 0;
  int actServID = 0;
	while((neibConnected < numOfNeighbrs) && (actServID < numOfServers))
	{
		if( neigLinks[actServID] <= 0)
		{
			actServID++;
			continue;
		}
		//create socket
		struct sockaddr_in con_addr;//creates structure con_addr
		int fdsocket = socket(AF_INET, SOCK_STREAM, 0);
		//Check if socket creation failed
		if( fdsocket < 0)
		{
			fprintf(stderr, "Error: Failed to create socket for server %d. Trying again.\n\n",
				actServID+1);
			//close(fdsocket);
			continue;
		}
		bzero(&con_addr, sizeof(con_addr));
		con_addr.sin_family = AF_INET;
		//inet_pton - convert IPv4 and IPv6 addresses from text to binary form
		inet_pton(AF_INET, servIPs[actServID], &con_addr.sin_addr);
		con_addr.sin_port = htons(servPorts[actServID]);

		while(connect(fdsocket, (struct sockaddr*)&con_addr, sizeof(con_addr))<0)
		{
			fprintf(stderr, "Error: Connect to server %d failed. Trying Again.\n\n", actServID+1);
			continue;
		}
		clientfds[actServID] = fdsocket;
		neigLinks[actServID] = 0;
		fprintf(stderr, "Connection to server %d successful!\n\n", actServID+1);
		actServID++;
		neibConnected++;
	}

  /**
	struct timespec {
        time_t   tv_sec;        // seconds
        long     tv_nsec;       //nanoseconds
  };**/
  int recvmap[numOfServers];
	for(int i = 0; i < numOfServers; i++)
	{
			recvmap[i]= -1;
	}


	//setting the time interalval to seconds
	//wait up to timeInterval seconds
	struct timeval timeInterval;
	timeInterval.tv_sec =  updateInterval;
	timeInterval.tv_sec = 0;

  //Initial topology
	int nextHop[numOfServers];
	int routingCost[numOfServers];


  int maxfd;
	//int selectSize;
	fd_set readfds; //fdset to find and store triggered fds from select
	int pakgCount;
  //Keep the server running
	while (1)
	{
			/* http://man7.org/linux/man-pages/man2/select.2.html */
			fd_set rfds; //fdset to find and store triggered fds from select
			 maxfd = fdSelect( serverfd, clientfds, recvfds, numOfServers, &rfds);
			//Modify readfds to have only triggered fds
			int retval = select(maxfd + 1, &rfds, NULL, NULL, &timeInterval);
			if(retval == -1)
			{
				////close all active connections
				for(int i = 0; i < numOfServers; i++)
				{

					clientfds[i] = 0;
					recvfds[i] = 0;
			  }
				perror("select()");
			}
			else if (retval == 0)
		  {
				printf("Data is available now.\n");
        /* FD_ISSET(0, &rfds) will be true. */
				fprintf(stderr, "---Automatic Update Starting---\n");
				for(int conectedNeigh = 0; conectedNeigh < numOfServers; conectedNeigh++)
				{
						if(clientfds[conectedNeigh] != 0)
						{
								if(neigLinks[conectedNeigh] == 1)
								{
										serMissed[conectedNeigh] = 0;

								}
								else
								{
										serMissed[conectedNeigh] = serMissed[conectedNeigh] + 1;
										if (serMissed[conectedNeigh] == MAXMISSES)
										{
												linkCost[conectedNeigh] = INF;
												costTable[serverID][conectedNeigh] = INF;
												costTable[conectedNeigh][serverID] = INF;
												//close clientfds[]
												clientfds[conectedNeigh] = 0;
												fprintf(stderr, "Error: Server %d is unresponsive.", conectedNeigh +1);
							          fprintf(stderr, " Closing link to this server.\n");
										}

								}
						}
						neigLinks[conectedNeigh] = 0;
				}
				sendhandler(numOfServers, 1, 0, serverID, 0, 0, 0, clientfds,
				linkCost, costTable);
				fprintf(stderr, "---Periodic Update Ending: Resetting timer.---\n");
				timeInterval.tv_sec =  updateInterval;
				timeInterval.tv_sec = 0;
		  }
			else
			{
				  int selectHand = 0;//fds satisfied
					if(FD_ISSET(FDSTDIN, &readfds) && (selectHand < retval ))
					{
							selectHand++;


					}
					if(FD_ISSET(serverfd, &readfds) && (selectHand < retval))
					{
						selectHand++;

					}
					if(selectHand < retval)
					{




					}
			}



	}

}
