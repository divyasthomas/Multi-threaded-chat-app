#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include "list.h"
//#include "mem.h"

#define MACHINE_NAME_LEN 80
#define MAX_LEN 5000
#define MAX_LIST_NODES 100

//for argvs
long int my_port_num; 
long int target_port_num; 
char machine_name[MACHINE_NAME_LEN]; 


struct sockaddr_in my_sin;
struct sockaddr_in target_sin;
struct hostent* my_info;
struct hostent* target_info;

int sockfd;//socket descripter
//threads used
pthread_t Receiver_thread, sender_thread, keyboard_thread, printer_thread;

List* receiver_list; // list used by receiver (producer) and printer (consumer)
List* sender_list; // list used by sender (consumer) and keyboard (producer)

static pthread_cond_t RList_cond_cons = PTHREAD_COND_INITIALIZER;
static pthread_cond_t RList_cond_prod = PTHREAD_COND_INITIALIZER;
static pthread_cond_t SList_cond_cons = PTHREAD_COND_INITIALIZER;
static pthread_cond_t SList_cond_prod = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cleaner_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock_R_P = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_S_K = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_cleaner = PTHREAD_MUTEX_INITIALIZER;
static bool exit_called = false;
static bool online = false;
static bool is_online = false;
char* exitc="!exit\n";
char* status="!status\n";
char* online_msg="Online\n";
int key=3;

void encryption(char* msg )
{
	char c;
	//int i;
	
	for(int i = 0; msg[i] != '\0'; ++i){
		c = msg[i];
		
		if(c >= 'a' && c <= 'z'){
			c = c + key;
			
			if(c > 'z'){
				c = c - 'z' + 'a' - 1;
			}
			
			msg[i] = c;
		}
		else if(c >= 'A' && c <= 'Z'){
			c = c + key;
			
			if(c > 'Z'){
				c = c - 'Z' + 'A' - 1;
			}
			
			msg[i] = c;
		}
	}
	
	//printf("Encrypted msg: %s", msg);
	return;
	
}


void decryption(char* msg )
{
	char c;
	//int i;
	
	
	for(int i = 0; msg[i] != '\0'; ++i){
		c = msg[i];
		
		if(c >= 'a' && c <= 'z'){
			c = c - key;
			
			if(c < 'a'){
				c = c + 'z' - 'a' + 1;
			}
			
			msg[i] = c;
		}
		else if(c >= 'A' && c <= 'Z'){
			c = c - key;
			
			if(c < 'A'){
				c = c + 'Z' - 'A' + 1;
			}
			
			msg[i] = c;
		}
	}
	
	//printf("Decrypted msg: %s", msg);
	
	return;
	
}



void* f_online() {
unsigned int sin_len = sizeof(target_sin);


 if (sendto(sockfd, online_msg, strlen(online_msg),0, (struct sockaddr*) &target_sin, sin_len) == -1) 
 		{   printf("online failed with errno %d\n", errno);  }

      online=false; //printf("online set to %d in func\n",online);    
  
        
  return NULL;
}


void* f_sender() {
  unsigned int sin_len = sizeof(target_sin);
  while (1) {

  //printf("in Tx\n");
  
    char* S_message;

    pthread_mutex_lock(&lock_S_K);
    {	
    	     	
     //printf("before while in Tx\n");
      while((List_count(sender_list) <= 0))
      { //printf("in while in TX\n");
        pthread_cond_wait(&SList_cond_cons, &lock_S_K);
      }
      	//printf("out of while in TX\n");
     
      void* S_message_pointer = List_trim(sender_list);
      S_message = (char*)S_message_pointer;
      
      //encrypt message
      encryption(S_message);
      //}
      //}
      if (sendto(sockfd,S_message, strlen(S_message),0, (struct sockaddr*) &target_sin, sin_len) == -1) {
          printf("sender failed with error: %d\n", errno);
        }
      pthread_cond_signal(&SList_cond_prod);
    }
    
    pthread_mutex_unlock(&lock_S_K);
    
    //decrypt message
   decryption(S_message);
   
   //checking if message is status
    if (!strcmp(S_message,status)) {
      sleep(1); //waiting to see if other process is running and sends packet back..
      if(!is_online)
      {      printf("Offline\n"); 
      }
      else is_online=false;
      
    } 
   
   //checking if message is exit
   if (!strcmp(S_message,exitc)) 
   {
      //printf("prog exited\n");
      free(S_message);

      pthread_mutex_lock(&lock_cleaner);
      {
        exit_called = true;
        pthread_cond_signal(&cleaner_cond);
      }
      pthread_mutex_unlock(&lock_cleaner);

      return NULL;
    }
 
	
	free(S_message);
  }
  return NULL;
}


void* f_receiver() {

  unsigned int sin_len = sizeof(target_sin);
  while (1) {
  
  //printf("in Rx\n");
    char R_message[MAX_LEN];
    int R_bytes = recvfrom(sockfd,R_message, MAX_LEN, 0,(struct sockaddr*) &target_sin, &sin_len);

//if nothing
    if (R_bytes == -1) {
      continue;
    }

    char* Rmessage_dyn = (char*)malloc(sizeof(char) * MAX_LEN);
    // adding null at end
    int last_pos = (R_bytes < MAX_LEN) ? R_bytes : MAX_LEN - 1;
    R_message[last_pos] = 0;
    
    //if message received is not online, decrypt it
    if (strcmp(R_message,online_msg))
    {decryption(R_message);}
    
    //copy
    strcpy(Rmessage_dyn,R_message);

    pthread_mutex_lock(&lock_R_P);
    {
      while(List_count(receiver_list) >= MAX_LIST_NODES) {
        pthread_cond_wait(&RList_cond_prod, &lock_R_P);
      }//printf("out of while in RX\n");
      
      //add message to list
      void* pItem = Rmessage_dyn;
      List_prepend(receiver_list, pItem);
      pthread_cond_signal(&RList_cond_cons);
    }
    pthread_mutex_unlock(&lock_R_P);

    if (!strcmp(R_message,exitc)) {
      return NULL;
    }
    
    //if message is !status
     if (!strcmp(R_message,status)) {
      online=true; //printf("online set to %d in Rx\n",online);
      f_online();
    }
    
   // if message is Online
 	 if (!strcmp(R_message,online_msg)) {
     is_online=true;
    }
    
  }
  
  return NULL;
}



void* f_printer() {
  while (1) {
   
   char* msg;
//printf("in display\n");
    pthread_mutex_lock(&lock_R_P);
    {
      while(List_count(receiver_list) <= 0) {
        pthread_cond_wait(&RList_cond_cons, &lock_R_P);
      } //printf("out of while in display\n");
      
      //taking message out of receiver list
      void* P_message_pointer = List_trim(receiver_list);
      msg = (char*)P_message_pointer;
      
      //if message is not status then print it
      if (strcmp(msg,status)) {
      fputs(msg, stdout);
      }
      
      //else printf("msg is status-not printing it..");
      //flush out stdout
      fflush(stdout);
      pthread_cond_signal(&SList_cond_prod);
    }
    pthread_mutex_unlock(&lock_R_P);

    if (List_count(receiver_list) == 0)
    {	 if (!strcmp(msg,exitc)) 
       { 
       	free(msg);
        //printf("other pg signalled exit\n");
        

        pthread_mutex_lock(&lock_cleaner);
        {
          exit_called = true;
          pthread_cond_signal(&cleaner_cond);
        }
        pthread_mutex_unlock(&lock_cleaner);

        return NULL;
      }
      
      
    }
    free(msg);
  }
  return NULL;
}



void* f_keyboard() {
  while (1) {
  
		//printf("in keyboard\n");
		 char user_msg[MAX_LEN];
	  
		//get message from user
		  if (fgets(user_msg, MAX_LEN, stdin) == NULL) 
		  {	  continue;
		  }
		  
	  
		 char* user_msg_dyn = (char*)malloc(sizeof(char) * MAX_LEN);
		 pthread_mutex_lock(&lock_S_K);
		 {	
		   //printf("inside lock\n");
		   while(List_count(sender_list) >= MAX_LIST_NODES) {
		     pthread_cond_wait(&SList_cond_prod, &lock_S_K);
		   	}
		   //printf("out of while in keyboard\n");
		   strcpy(user_msg_dyn,user_msg);
		 
		   void* pItem = user_msg_dyn;
		   List_prepend(sender_list, pItem);
		   pthread_cond_signal(&SList_cond_cons);
		 }
		 pthread_mutex_unlock(&lock_S_K);
		 
	 	if (!strcmp(user_msg,exitc)) 
		 { 
		   return NULL;
		 }

  } 
  //free(user_msg_dyn);
  return NULL;
}



int main(int argc, char const** argv) 
{
  if (argc != 4) 
  {
    printf("Error: Some input missing.. enter all inputs: ./s-talk [local port] [remote machine name] [remote port].\n");
    return -1;
  }
  
  else if (argc == 4) 
  {
    my_port_num = atoi(argv[1]);
    strcpy(machine_name, argv[2]);
    target_port_num = atoi(argv[3]);
  }

   // make socket
  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) 
  {
    printf("Failed to create socket. Error: %d\n", errno);
    exit(-1);
  }
  
  // Target address 
  memset(&target_sin, 0, sizeof(target_sin));
  target_info = gethostbyname(machine_name);
  if (target_info == NULL) 
  {
    memset(&my_sin, 0, sizeof(my_sin));
    memset(&target_sin, 0, sizeof(target_sin));
    printf("Error: Remote computer not found\n");
    exit(-1);
  }
  
  memcpy((void*) &target_sin.sin_addr, target_info->h_addr_list[0], target_info->h_length);
  target_sin.sin_port = htons(target_port_num);
  

  // my address 
  memset(&my_sin, 0, sizeof(my_sin));
  my_sin.sin_family = AF_INET; 
  char my_name[MACHINE_NAME_LEN];
  gethostname(my_name, MACHINE_NAME_LEN); 
  my_info = gethostbyname(my_name); 
  memcpy((void*) &target_sin.sin_addr, my_info->h_addr_list[0], my_info->h_length);
  my_sin.sin_port = htons(my_port_num); 


  // Bind the socket and print error
  if (bind(sockfd, (struct sockaddr*) &my_sin, sizeof(my_sin)) == -1) {
    memset(&my_sin, 0, sizeof(my_sin));
    memset(&target_sin, 0, sizeof(target_sin));
    printf("Bind failed. Error: %d\n", errno);
    exit(-1);
  }

//welcome message
 printf("welcome to Lets-Talk! Please Type your messages now\n");


  // Create lists 
  receiver_list = List_create();
  sender_list = List_create();
  

  // Create threads
  pthread_create(&Receiver_thread, NULL, f_receiver, NULL);
  pthread_create(&sender_thread, NULL, f_sender, NULL);
  pthread_create(&keyboard_thread, NULL, f_keyboard, NULL);
  pthread_create(&printer_thread, NULL, f_printer, NULL);
  
//waiting for exit

while (1) 
  { //printf("hhhh exited\n");
    pthread_mutex_lock(&lock_cleaner);
    { //printf("before while exited\n");
      while(!exit_called) {
        pthread_cond_wait(&cleaner_cond, &lock_cleaner);
      }
 //printf("after while exited\n");
      // Cancel threads
      pthread_cancel(Receiver_thread);
      pthread_cancel(keyboard_thread);
      pthread_cancel(sender_thread);
      pthread_cancel(printer_thread);

      // Close socket
      if (close(sockfd) == -1) {
        printf("Can't close socket. Error: %d\n", errno);
      }

      pthread_mutex_unlock(&lock_cleaner);
      break;
    }
  }
  

  // Join threads
  pthread_join(Receiver_thread, NULL);
  pthread_join(sender_thread, NULL);
  pthread_join(keyboard_thread, NULL);
  pthread_join(printer_thread, NULL);
    
  
  memset(&my_sin, 0, sizeof(my_sin));
  memset(&target_sin, 0, sizeof(target_sin));
  
  // destroy List
  List_free(receiver_list, free);
 List_free(sender_list, free);

  // Destory condition variables
  pthread_cond_destroy(&RList_cond_cons);
  pthread_cond_destroy(&RList_cond_prod);
  pthread_cond_destroy(&SList_cond_cons);
 pthread_cond_destroy(&SList_cond_prod);
 pthread_cond_destroy(&cleaner_cond);

  //Destory mutex
  pthread_mutex_destroy(&lock_R_P);
  pthread_mutex_destroy(&lock_S_K);
  pthread_mutex_destroy(&lock_cleaner);
  
   
  
  return 0;
}
