#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/fcntl.h>

#define LINESIZE 256


/********************** strcuts defination **********************/////////////////
typedef struct router
{
	char* routername; // The router's name
	char* ip; // The router's IP
	int port; // the Router's port
	int index; // what index it holds on my array?
	int old_index; // before update... after update....
	int* DV; // it's own distance vector, the main router defaults to file configuration, the rest are there for update
	int isNeighbor; // 1 if is neighbor, 0 otherwise
	struct router** arr; // the array itself, so each rotuer can find his friend's data such as IP and port...
	int main_index; // who's the main index called on the terminal?
	int* update; // who was updated on our terminals
	int* done; // a flag that the calculator tells everyone that it's time to close
	int amount_of_neighbors; // the amount of routers who are neighbors
	int amount_of_routers; // the amount of routers AT ALL
	int reconnect_tries; // reconnect retries
	char** via; // who did I pass from to update my DV at i? relevent to calculator and main index only, but its a shared pointer
	int* senders_count;
	pthread_cond_t* calccv; // calculator's cv
	pthread_mutex_t* calclock; // calculators cv's lock
	pthread_mutex_t* lock2; // reciever's lock
	pthread_mutex_t sender_lock; // sender's lock
	pthread_cond_t senders_cv; // sender's CV
	int* online;
}router;



///////********************* function defination***********************/////////////
int check_input(char* num);
int attempt_read(char* linebuffer, FILE* file);
void empty_string(char* str);
int initate_routers(router*** arr, int amount_of_routers,char*** viaa, int reconnect_tries, pthread_cond_t* calc_cv, pthread_mutex_t* calc_lock, int* done, pthread_mutex_t* ticket_lock , int* senders_count);
void print_routers(router** arr, int amount);
void free_routers(router** arr, int amount, pthread_t* senders, pthread_t* recievers, char** router_names);
int get_router_data(FILE* file, router** arr, char* buffer, char*** names);
int update_my_indexes(router** arr, char* routersname);
int get_router_data(FILE* file, router** arr, char* buffer, char*** namess);
int neighbor_initation(router** arr);
int reorder_arr(router*** arr, router*** arr2);
int initate_DV(FILE* file, router** arr, char* buffer, char* routersname, char** via);
int threadding_data(pthread_t** senderss, pthread_t** recieverss, router** arr);
void* sender(void* args);
void* reciever(void* args);
void* calculator(void* args);
int ascii_sum(char* n);

int main(int argc, char** argv)
{
	if(argc != 4)
	{
		fprintf(stderr, "Not enough arguments, please use the command ./%s <file input name> <Router's name> <amount of reconnect tries>\n",argv[0]); // parameters must be 4 inc exec name
		return -1;
	}
	signal(SIGPIPE, SIG_IGN); // probably possible to write into a SD when other thread closed it
	/******************************* arguments decleration************************/
	int i,j; // for loops

	char* filename=0; // parameter argv[1]
	FILE* file; // the actual file pointer
	char line[LINESIZE]; // the file buffer
	char* routername=0; // parameter argv[2]
	int reconnect_tries=0; // parameter argv[3]

	/////****** router struct and router struct loop variables ****/////
	///**** all routers****////
	router** all_routers=0; // all the routers, stage before finding neighbors
	int amount_of_routers;
	/////*** neighbors only***///
	router** routers=0; // routers data holding ONLY neighbors
	int amount_of_neighbors;
	//// relevent index for this router///
	int main_index; // where's THIS router located at?
	int senders_count=0;
	///////****** printing at the end variables ****////
	char** all_router_names=0; // all router names, for printing at the end....
	char** via=0; // via array, for printing only

	///////**** thread opening variables *****////////
	pthread_t* senders=0;
	pthread_t* recievers=0;
	pthread_t calcid;



	////////********* thread synchronizing variables *****/////
	int done = 0;
	pthread_mutex_t ticket_lock;
	pthread_cond_t calc_cv;
	pthread_mutex_t calc_lock;

	///////////////////******************* EMD OF argument decleration ************************////////////////


	/////////////////************************* input initation********************************///////////////
	filename = argv[1];
	routername = argv[2];
	if(!check_input(argv[3]))
	{
		fprintf(stderr, "%s\n", "invalid reconnect times argument");
		return -1;
	}
	reconnect_tries = atoi(argv[3]);
	if(reconnect_tries<=0)
	{
		fprintf(stderr, "%s\n", "reconnect tries must be a positive integer");
		return -1;
	}
	file = fopen(filename,"r");
	if(!file)
	{
		fprintf(stderr, "%s\n", "file reading error, no access or file doesn't exist");
		return -1;
	}

	if(!attempt_read(line,file))
		return -1;
	if(!check_input(line))
	{
		fprintf(stderr, "%s\n", "invalid input for amount of routers\n");
		return -1;
	}
	amount_of_routers = atoi(line);
	if(amount_of_routers<=0)
	{
		fprintf(stderr, "%s\n", "amount of routers must be a positive integer");
		return -1;
	}
	empty_string(line);
	///////////////////////////******************** END INPUT INITATION STAGE **************///////////////



	/////////////////////************** initating routers stage *****************////////////
	if(!initate_routers(&all_routers, amount_of_routers, &via, reconnect_tries, &calc_cv, &calc_lock, &done, &ticket_lock,&senders_count))
	{
		return -1;
	}
	//////********************** END of router initation stage ******************//////////////////////



	////////////////*************** grabbing router's data stage ****************/////////////////
	if(!get_router_data(file, all_routers, line,&all_router_names))
	{
		free_routers(all_routers, amount_of_routers,senders,recievers,all_router_names);
		fclose(file);
		return -1;
	}
	/////////////////////////*************** finding this router's name on array*******////////////
	if(!update_my_indexes(all_routers, routername))
	{
		free_routers(all_routers, amount_of_routers,senders, recievers,all_router_names);
		fclose(file);
		return -1;
	}
	////////////////////********* Initating the DV stage ********************//////////////////////////
	if(!initate_DV(file, all_routers, line, routername,via))
	{
		free_routers(all_routers, amount_of_routers,senders, recievers,all_router_names);
		fclose(file);
		return -1;
	}
	fclose(file);
	//////////////////****** initating my own DV to be 0 ************////////////////////////
	all_routers[all_routers[0]->main_index]->DV[all_routers[0]->main_index]=0;
	//////////////////////// ************ neighbor data count **********/////////////////
	if(!neighbor_initation(all_routers))
	{
		free_routers(all_routers, amount_of_routers,senders, recievers,all_router_names);
		return -1;
	}
	//////////////////////*************** neighbor re-ordering *********/////////////////
	if(!reorder_arr(&all_routers, &routers))
	{
		free_routers(all_routers, amount_of_routers,senders, recievers,all_router_names);
		return -1;
	}
	amount_of_neighbors = routers[0]->amount_of_neighbors;
	main_index = routers[0]->main_index;
	routers[0]->update[main_index] = 1;
	for(i=0; i<amount_of_neighbors+1;i++)
		routers[i]->arr = routers;
	/////////////////************* creates a pthread_t in the right size for both senders and recievers****/////
	if(!threadding_data(&senders, &recievers, routers))
	{
		free_routers(routers, amount_of_neighbors+1,senders, recievers,all_router_names);
		return -1;
	}

	//////////////********************* initates mutex and cv for calculator and ticket ********////////
	pthread_mutex_init(&ticket_lock, NULL);
	pthread_mutex_init(&calc_lock, NULL);
	pthread_cond_init(&calc_cv, NULL);
	////////////*********** pthread_create **************/////////////
	j=0;
	for(i=0; i<amount_of_neighbors+1; i++)
	{
		if(i!=main_index)
		{
			pthread_create(senders+j, NULL, sender, (void*) routers[i]);
			pthread_create(recievers+j, NULL, reciever, (void*) routers[i]);
			j++;
		}
	}
	pthread_create(&calcid,NULL,calculator,(void*) routers[main_index]);

	//////////////********************* pthread join *************////////////////////

	for(i=0; i<amount_of_neighbors; i++)
	{
		pthread_join(senders[i],NULL);
		pthread_join(recievers[i],NULL);
	}
	pthread_join(calcid,NULL);


	for(i=0; i<amount_of_neighbors+1; i++)
	{
		if(i!=main_index)
		{
			pthread_mutex_destroy(&routers[i]->sender_lock);
			pthread_cond_destroy(&routers[i]->senders_cv);
		}
	}
	int* dv = routers[main_index]->DV;
	for(i=0; i<amount_of_routers;i++)
	{
                printf("To %s Via ",all_router_names[i]);
		if(via[i])
		        printf("%s ",via[i]);
		else
		        printf("NULL ");
		printf("Weight "); 
		if(dv[i] == INT_MAX)
		        printf("Infinity\n");
		else
		        printf("%d\n",dv[i]);
	}
	pthread_mutex_destroy(&ticket_lock);
	pthread_mutex_destroy(&calc_lock);
	pthread_cond_destroy(&calc_cv);
	free_routers(routers, amount_of_neighbors+1, senders, recievers, all_router_names);
}
/*
	initates thread sync data
*/
int threadding_data(pthread_t** senderss, pthread_t** recieverss, router** arr)
{
	int amount_of_neighbors = arr[0]->amount_of_neighbors;
	int i;
	pthread_t* senders = (pthread_t*) calloc (amount_of_neighbors, sizeof(pthread_t));
	if(!senders)
		return 0;
	pthread_t* recievers = (pthread_t*) calloc (amount_of_neighbors, sizeof(pthread_t));
	if(!recievers)
	{
		free(senders);
		return 0;
	}
	senderss[0] = senders;
	recieverss[0] = recievers;
	int* online = (int*) calloc (amount_of_neighbors+1,sizeof(int));
	if(!online)
		exit(-1);
	for(i=0; i<amount_of_neighbors+1; i++)
	{
		online[i] = 1;
		arr[i]->online = online;
	}
	return 1;
}
/*
	reorders the neighbor array 
*/
int reorder_arr(router*** arr, router*** arr2)
{
	router** all_routers = arr[0];
	int amount_of_neighbors = all_routers[0]->amount_of_neighbors;
	int amount_of_routers = all_routers[0]->amount_of_routers;
	int me = all_routers[0]->main_index;
	int k=0,i;
	////// ****** alllcates new array at neighbor size + me *********///////
	router** neighbors = (router**) calloc (amount_of_neighbors+1, sizeof(router*));
	arr2[0] = neighbors;
	if(!neighbors)
	{
		fprintf(stderr, "%s\n", "malloc error");
		return 0;
	}
	///////////********** to remember the master.... ************//////////////////
	char* name_me = all_routers[me]->routername;
	/////////////*********** runs on all the routers **********///////
	for(i=0; i<amount_of_routers;i++)
	{
		if(i==me) // case me, remember me
		{
			neighbors[k] = all_routers[i];
			k++;
		}
		else if(all_routers[i]->isNeighbor) // case neighbor, remember neighbor
		{
			neighbors[k] = all_routers[i];
			k++;
		}
		else // case else, free everything except name
		{
			free(all_routers[i]->DV);
			free(all_routers[i]->ip);
			free(all_routers[i]);
		}
	}
	free(all_routers);
	arr[0] = 0;
	////////////// ************ redo the indexes ***********/////////////////
	for(i=0; i<amount_of_neighbors+1; i++)
	{
		if(!strcmp(name_me,neighbors[i]->routername))
			break;
	}
	me = i;
	for(i=0; i<amount_of_neighbors+1; i++)
	{
		neighbors[i]->old_index = neighbors[i]->index;
		neighbors[i]->index = i;
		neighbors[i]->main_index = me;
		if( i != me)
		{
			pthread_mutex_init(&neighbors[i]->sender_lock, NULL);
			pthread_cond_init(&neighbors[i]->senders_cv, NULL);
		}
	}
	return 1;
}
/*
	actual algorithm calculation, runs ONCE per process, unlike the threads which run at 2*(amount of closure of neighbors)
*/
void* calculator(void* args)
{
	int i,j;
	router* r = (router*) args;
	router** arr = (router**) r->arr;
	int me = r->main_index;
	int change;
	char** via = r->via;
	int* dv = r->DV;
	int amount_of_routers = r->amount_of_routers;
	int amount_of_neighbors = r->amount_of_neighbors;
	int max;
	while(1)
	{
		//printf("calculator\n");
		pthread_mutex_lock(r->calclock);
		pthread_cond_wait(r->calccv,r->calclock);
		pthread_mutex_unlock(r->calclock);
		//while(r->senders_count[0] != r->amount_of_neighbors);
		//r->senders_count[0] = 0;
		change=0;
		for(i=0; i<amount_of_neighbors+1; i++)
		{
		      //  if(r->update[i])
		        //{
			        for(j=0; j<amount_of_routers; j++)
			        {
				        if(dv[j] > dv[arr[i]->old_index] + arr[i]->DV[j] && dv[arr[i]->old_index] + arr[i]->DV[j] >=0)
				        {
					        change=1;
					        if(strcmp(arr[i]->routername,arr[me]->routername))
					                via[j] = via[arr[i]->old_index];
					        dv[j] = dv[arr[i]->old_index] + arr[i]->DV[j];
				        }
			        }
			        if(dv[arr[i]->old_index] > arr[i]->DV[r->old_index])
			        {
				        change=1;
				        if(strcmp(arr[i]->routername,arr[me]->routername))
				               via[arr[i]->old_index] = arr[i]->routername;
				        dv[arr[i]->old_index] = arr[i]->DV[r->old_index];
			        }
			//}
		}
		r->update[me] = change;
		max=0;
		for(i=0; i<amount_of_neighbors+1; i++)
		{
			if(r->update[i])
				max=1;
		}
		for(i=0; i<amount_of_neighbors+1; i++)
		{
			if(i!=me)
				r->update[i]=-1;
		}
		if(!max)
			r->done[0]=1;
		sleep(1);
		for(i=0; i<amount_of_neighbors+1; i++)
		{
			if(i!=me)
				pthread_cond_signal(&arr[i]->senders_cv);
		}
		if(!max)
			break;
	}
	return NULL;
}
/*
	sender thread - acts as indepdent router's network card sending subroutine
*/
void* sender(void* args)
{
	router* r = (router*) args;
	router** arr = (router**) r->arr;
	int me = r->main_index;
	int number_of_tries = r->reconnect_tries;
	int i;
	struct sockaddr_in sin;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); // SOCKET TCP/IP
	if(sockfd == -1)
	{
	    perror("socket");
	    return NULL;
	}
	int sum = ascii_sum(arr[me]->routername);
	sin.sin_port = htons(r->port + sum); // PORT I need to conenct to is my ASCII sum + destination port
	sin.sin_addr.s_addr = inet_addr(r->ip); // struct holds the IP I need to connect to
	sin.sin_family = AF_INET; /// IPv4
	memset(sin.sin_zero, '\0', sizeof sin.sin_zero);
	for(i=0; i<number_of_tries; i++)
	{
		if(connect(sockfd, (struct sockaddr*) &sin, sizeof(sin))==-1) // tries to connect
			sleep(1);
		else// upon success
			break; // exit the constant tries
	}
	if( i ==number_of_tries) // if I tried ALL my connection tries and I COULDN'T connect
	{
		perror("connect"); // there was an error connecting
		close(sockfd);
	    return NULL;
	}
	while(1)
	{
		if(r->update[r->main_index] == 0)
		{
			char c = '0';
			if(send(sockfd, &c, sizeof(char) ,0)<0)
			{
			        fprintf(stderr, "some error occured with sockets\n");
			        break;
			}
		}
		else
		{
			char c='1';
			if(send(sockfd, &c, sizeof(char) ,0)<0)
			{
			        fprintf(stderr, "some error occured with sockets\n");
			        break;
			}
			char* buffer = (char*) arr[r->main_index]->DV;
			int remaining = sizeof(int) * r->amount_of_routers;
			while(remaining>0)
			{
				int sent = send(sockfd, buffer, remaining,0);
				if(sent<0)
				{      
				         fprintf(stderr, "some error occured with sockets\n");
				        close(sockfd);
				        return NULL;
				}
				remaining-=sent;
				buffer+=sent;
			}
		}
		pthread_mutex_lock(r->lock2);
		r->senders_count[0]++;
		pthread_mutex_unlock(r->lock2);
		pthread_mutex_lock(&r->sender_lock);
		pthread_cond_wait(&r->senders_cv, &r->sender_lock);
		sleep(1);
		pthread_mutex_unlock(&r->sender_lock);
		if(r->done[0])
			break;

	}
        char c = '0';
	send(sockfd, &c, sizeof(c), 0);
	close(sockfd); // closes the FD
	return NULL;
}

/*
	reciever thread - acts as a independednt router's network card recieving call
*/
void* reciever(void* args)
{
	router* r = (router*) args;
	router** arr = (router**) r->arr;
	int me = r->main_index;
	int i,result;
	int listenfd=0,connfd=0;
	struct timeval timeout;
	struct sockaddr_in serv_addr;
	char c;
	fd_set readset;
	listenfd = socket(AF_INET, SOCK_STREAM, 0); /// TCP,IP
	if(listenfd == -1)
	{
	    perror("socket");
	    return NULL;
	}
	memset(&serv_addr, '0', sizeof(serv_addr));
	int sum = ascii_sum(r->routername);
	serv_addr.sin_port = htons(arr[me]->port + sum); // calculated port
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // all the IP's
	serv_addr.sin_family = AF_INET; // TCP on IPv4
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0) // binds the socket, "catching" it for myself only
	{
		perror("bind");
		return NULL;
	}
	if(listen(listenfd, 10)<0) // more server initation, listening to that port at 10 interval
	{
		perror("listen\n");
		return NULL;
	}
	connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); /// WAITING FOR A CLIENT TO MAKE CONNECTION
	close(listenfd);
	while(!r->done[0])
	{
		FD_ZERO(&readset);
		FD_SET(connfd, &readset);
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
		result = select(connfd+1,&readset,NULL,NULL,&timeout);
		if(result)
		{
			c='a';
			if(recv(connfd,&c,sizeof(char), 0)==0)
			{
			        break;
			}
			if(c=='0')
			{
				r->update[r->index] = 0;
			}
			else if(c=='1')
			{
				r->update[r->index] = 1;
				char* buffer = (char*) r->DV;
				int remaining = sizeof(int) * r->amount_of_routers;
				while(remaining>0)
				{
					int recvd = recv(connfd,buffer,remaining,0);
					if(recvd<0)
					{
					        fprintf(stderr, "some error occured with sockets\n");
					        break;
					}
					remaining-=recvd;
					buffer +=recvd;
				}
			}
			pthread_mutex_lock(r->lock2);
			int check=1;
			for(i=0; i<r->amount_of_neighbors+1; i++)
			{
				if(r->update[i]==-1 && r->online[i])
					check=0;
			}
			if(check)
			{
				pthread_cond_signal(r->calccv);
				sleep(1);
			}
			pthread_mutex_unlock(r->lock2);
		}
		else if(result==-1)
			break;
	}
	r->online[r->index] = 0;
	close(connfd);
	return NULL;
}
/*
	function to calculate the ASCII SUM value of a string, purposes of port mapping
*/
int ascii_sum(char* n)
{
	int sum=0;
	while(*n!='\0')
	{
		sum+=(unsigned int) *n;
		n++;
	}
	return sum;
}
/*
	subroutine initates the neighbor data, clearing the routers who are not part of closure of neighbors out of the struct array
*/
int neighbor_initation(router** arr)
{
	int amount_of_routers = arr[0]->amount_of_routers;
	int i;
	int amount_of_neighbors=0;
	int* update=0;
	///// ********** counts the neighbors ********///////
	for(i=0; i<amount_of_routers;i++)
	{
		if(arr[i]->isNeighbor)
			amount_of_neighbors++;
	}
	//////// ********** creates an update array *******//////
	update = (int*) calloc (amount_of_neighbors+1,sizeof(int));
	if(!update)
	{
		fprintf(stderr, "%s\n", "malloc error");
		return 0;
	}
	////// ******** updates everyone to that count **********/////////
	for(i=0; i<amount_of_routers;i++)
	{
		arr[i]->amount_of_neighbors = amount_of_neighbors;
		arr[i]->update = update;
	}
	for(i=0; i<amount_of_neighbors+1; i++)
	{
		update[i]=-1;
	}
	return 1;

}
/*
	subroutine responisble of finding each router an dupdating the indexes on the struct array
*/
int update_my_indexes(router** arr, char* routersname)
{
	int amount_of_router = arr[0]->amount_of_routers;
	int i;
	for(i=0; i<amount_of_router; i++)
	{
		if(!strcmp(routersname,arr[i]->routername))
			break;
	}
	if(i==amount_of_router)
	{
		fprintf(stderr, "%s\n", "couldn't find this router");
		return 0;
	}
	int j;
	for(j=0; j<amount_of_router;j++)
	{
		arr[j]->main_index=i;
	}
	return 1;
}
/*
	subroutine responsible of calculating distance vector initation, before the algorithm starts, to create a closure for each router
*/

int initate_DV(FILE* file, router** arr, char* buffer, char* routersname, char** via)
{
	int amount_of_router = arr[0]->amount_of_routers;
	int me = arr[0]->main_index;
	char* left=0;
	char* right=0;
	char* DV=0;
	empty_string(buffer);
	int i;
	while(attempt_read(buffer, file))
	{
		//////****** grabs the left data***********//////////////
		left = strtok(buffer, " ");
		if(!left)
		{
			//fprintf(stderr, "%s\n", "left router's name input error on file");
			break;
		}
		/////////////*********** grabs the right data *********/////////
		right=strtok(NULL, " ");
		if(!right)
		{
			//fprintf(stderr, "%s\n", "right router's name input error on file");
			break;
		}
		///////////// ************ grabs the DV ************//////////
		DV = strtok(NULL, " ");
		if(!DV)
		{
			//fprintf(stderr, "%s\n", "DV input error on file");
			break;
		}
		if(!check_input(DV))
			return 0;
		int dv_i = atoi(DV);
		if(dv_i <0)
		{
			fprintf(stderr, "%s\n", "unable to parse DV below 0");
			break;
		}

		///////////////******************* 3 cases of the left and right data, the router's name is on the left side, on the right side or neither ***///
		///// input check //////
		if(!strcmp(left,right))
		{
				fprintf(stderr, "%s\n", "unable to parse a DV from the same destination to the same destination");
				return 0;
		}
		//// case left data : a new DV and neighbor////
		else if(!strcmp(routersname,left))
		{
			for(i=0; i<amount_of_router;i++)
			{
				if(!strcmp(arr[i]->routername,right))
					break;
			}
			if(i==amount_of_router)
				return 0;
			arr[i]->isNeighbor=1;
			if(arr[me]->DV[i]!=INT_MAX)
			{
				fprintf(stderr, "%s\n", "unable to parse multiple DV's from the same source to the same destination");
				return 0;
			}
			arr[me]->DV[i] = dv_i;
		}
		///// case right data : a neighbor ////
		else if(!strcmp(routersname,right))
		{
			for(i=0; i<amount_of_router;i++)
			{
				if(!strcmp(arr[i]->routername,left))
					break;
			}
			if(i==amount_of_router)
				return 0;
			arr[i]->isNeighbor=1;
		}

		empty_string(buffer);
	}
	for(i=0; i<amount_of_router;i++)
	{
	        if(arr[me]->DV[i]!=INT_MAX)
	            via[i]=arr[i]->routername;
	        else
	            via[i]=0;
	}
	return 1;
}

/*
	subroutine responsible of grabbing router data from input file
*/
int get_router_data(FILE* file, router** arr, char* buffer, char*** namess)
{
	int amount_of_router = arr[0]->amount_of_routers;
	char** names = calloc(amount_of_router,sizeof(char*));
	if(!names)
		return 0;
	namess[0] = names;
	char* temp=0;
	int i;
	for(i=0; i<amount_of_router; i++)
	{
		empty_string(buffer);
		if(!attempt_read(buffer, file))
		{
			free(names);
			return 0;
		}
		///// ************ grab the router's name ********/////////
		temp = strtok(buffer, " ");
		if(!temp)
		{
			free(names);
			fprintf(stderr, "%s\n", "unable to grab Router's name");
			fclose(file);
			return 0;
		}
		///////*********** allocates space for router's name ********/////
		arr[i]->routername = (char*) calloc(strlen(temp)+1,sizeof(char));
		if(!arr[i]->routername)
		{
			free(names);
			fprintf(stderr, "%s\n", "malloc");
			fclose(file);
			return 0;
		}

		////******* copies router's name******************//////////
		strcpy(arr[i]->routername,temp);
		names[i] = arr[i]->routername;
		////////////////************ grabs the IP **********//////////
		temp = strtok(NULL, " ");
		if(!temp)
		{
			free(names);
			fprintf(stderr, "%s\n", "unable to grab Router's IP address");
			fclose(file);
			return 0;
		}
		///////// ************ allocates space for IP ******//////////////
		arr[i]->ip = (char*) calloc ( strlen(temp)+1,sizeof(char));
		if(!arr[i]->ip)
		{
			free(names);
			fprintf(stderr, "%s\n", "malloc");
			fclose(file);
			return 0;
		}
		/////////////////******copes router's IP****************/////////////
		strcpy(arr[i]->ip,temp);

		////////// ***************** grabs the port ***********//////////////
		temp=strtok(NULL, " ");
		if(!temp)
		{
			free(names);
			fprintf(stderr, "%s\n", "unable to grab Router's port");
			fclose(file);
			return 0;
		}
		//////////////////////// ************** check's port input **********//////////
		if(!check_input(temp))
		{
			free(names);
			fprintf(stderr, "%s\n", "malloc");
			fclose(file);
			return 0;
		}
		//////////////////// ****************** copies the port ***********//////////
		arr[i]->port = atoi(temp);
		if(arr[i]->port <=0)
		{
			fprintf(stderr, "%s\n", "port has to be above 0");
			{
				free(names);
				fprintf(stderr, "%s\n", "malloc");
				fclose(file);
				return 0;
			}
		}
	}
	return 1;
}
/*
	function made for deugging purposes, prints the routers data
*/
void print_routers(router** arr, int amount)
{
	int i;
	for(i=0; i<amount; i++)
	{
		printf("Router's name is %s",arr[i]->routername);
		printf(", Router's IP is %s",arr[i]->ip);
		printf(" , Router's index is %d", arr[i]->index);
		printf(" Router's port is %d",arr[i]->port);
		if(arr[i]->isNeighbor)
			printf(", Router is a neighbor");
		else
			printf(", Router is NOT a neighbor");
		printf("\n");
	}
	printf("\nDistance vector: ");
	int amount_of_routers = arr[0]->amount_of_routers;
	for(i=0; i<amount_of_routers; i++)
	{
		printf("%d ",arr[arr[0]->main_index]->DV[i]);
		if(i+1<amount_of_routers)
			printf(" , ");
	}
	printf("\n\n");
}
/*
	subroutine responsible of freeing routers structs data and its memory allocation
*/
void free_routers(router** arr, int amount, pthread_t* senders, pthread_t* recievers, char** router_names)
{
	if(!arr)
		return;
	int i;
	int amount_of_routers = arr[0]->amount_of_routers;
	char** via = arr[0]->via;
	int* update = arr[0]->update;
	int* online = arr[0]->online;
	for(i=0; i<amount; i++)
	{
		if(!arr[i])
			continue;
		if(arr[i]->DV)
			free(arr[i]->DV);
		if(arr[i]->ip)
			free(arr[i]->ip);
		if(arr[i])
			free(arr[i]);
	}
	if(senders)
		free(senders);
	if(recievers)
		free(recievers);
	if(via)
		free(via);
	for(i=0; i<amount_of_routers;i++)
	{
		if(router_names[i])
			free(router_names[i]);
	}
	if(router_names)
		free(router_names);
	if(update)
		free(update);
	if(online)
		free(online);
	free(arr);
}
/*
	subroutine responsible initating the router struct and it's data, along with allocation
*/
int initate_routers(router*** arr, int amount_of_routers,char*** viaa, int reconnect_tries, pthread_cond_t* calc_cv, pthread_mutex_t* calc_lock, int* done, pthread_mutex_t* ticket_lock,int* senders_count  )
{
	int i;
	router** all_routers=0;
	all_routers = (router**) calloc(amount_of_routers,sizeof(router*));
	if(!all_routers)
	{
		fprintf(stderr, "%s\n", "malloc error");
		return 0;
	}
	char** via =(char**) calloc (amount_of_routers,sizeof(char*));
	if(!via)
	{
		fprintf(stderr, "%s\n", "malloc error");
		free(all_routers);
		return 0;
	}
	viaa[0] = via;
	arr[0] = all_routers;
	for(i=0; i<amount_of_routers; i++)
	{
		all_routers[i] = (router*) malloc(sizeof(router));
		if(!all_routers[i])
		{
			fprintf(stderr, "%s\n", "malloc error");
			free(all_routers);
			return 0;
		}
		all_routers[i]->senders_count = senders_count;
		all_routers[i]->arr = all_routers;
		all_routers[i]->amount_of_routers = amount_of_routers;
		all_routers[i]->isNeighbor=0;
		all_routers[i]->DV = (int*) calloc (amount_of_routers, sizeof(int));
		if(!all_routers[i]->DV)
		{
			fprintf(stderr, "%s\n", "malloc error");
			free_routers(all_routers, amount_of_routers, NULL, NULL, NULL);
			return 0;
		}
		int j;
		for(j=0; j<amount_of_routers;j++)
			all_routers[i]->DV[j] = INT_MAX;
		all_routers[i]->via = via;
		all_routers[i]->reconnect_tries = reconnect_tries;
		all_routers[i]->calccv = calc_cv;
		all_routers[i]->calclock = calc_lock;
		all_routers[i]->lock2 = ticket_lock;
		all_routers[i]->index=i;
		all_routers[i]->done = done;
		all_routers[i]->port=0;
		all_routers[i]->routername=0;
		all_routers[i]->update=0;
		all_routers[i]->main_index=0;
		all_routers[i]->online=0;
	}
	return 1;
}
/*
	subroutine to initate string to null terminators, string must be LINESIZE long
*/
void empty_string(char* str)
{
	int i;
	for(i=0; i<LINESIZE; i++)
	{
		*str = '\0';
		str++;
	}
}
/*
	attempts to read the file, returns 1 if succesfull, 0 if failed
*/
int attempt_read(char* linebuffer, FILE* file)
{
	if(!fgets(linebuffer,LINESIZE,file))
	{
		//fclose(file);
		return 0;
	}
	return 1;
}
/*
	subroutine responsible for checking input is indeed a number
*/
int check_input(char* num)
{
	int counter=0;
	while(*num!='\0' && *num!='\n')
	{
		if((*num<'0' || *num>'9') && *num!=' ')
			return -1;
		else if(*num == ' ')
		{

		}
		else
			counter++;
		num++;
	}
	if(!counter)
		return 0;
	return 1;
}

