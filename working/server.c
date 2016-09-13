#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include "util.h"

#undef ZMQ
#undef NEED_CRANKS
#undef SET_SCOK

#ifdef ZMQ
#include <zmq.h>
#endif

#define BATTERY_LEVEL_HANDLE 		0x002c
#define SPEED_HANDLE			0x0012
#define LINK_LOSS			0x1803 // UUID. We don't know the exact handle. 

#define WHEEL_REVOLUTIONS_PRESENT	0x01
#define CRANK_REVOLUTIONS_PRESENT	0x02

#define VEHICLEID 1
#define PI	  3.14

uint32_t 	wheel_revolutions;
uint16_t	last_wheel_rev_time;
uint32_t 	prev_wheel_revolutions;
uint16_t	mydifftime;
uint32_t        num_wheel_revolutions;
uint32_t 	num_crank_revolutions;
uint32_t	crank_revolutions;
uint32_t	last_crank_rev_time;
uint32_t	prev_crank_revolutions;
uint32_t	prev_last_wheel_rev_time;
uint32_t	prev_last_crank_rev_time;
uint16_t 	mycranktime;
uint8_t         flags;
float 		distance;
uint32_t 	speed;
float		total_distance;
uint8_t		battery_level;
unsigned char pdu[21];
uint16_t handle;
float 		diameter;
uint8_t 	vehicle_id;

void generate_html_page() {

  FILE * fp;

  fp = fopen ("index.html", "w");
  fprintf (fp,"<html>\n");
  fprintf (fp,"<body bgcolor=""#CC00CC"">\n");
  fprintf (fp,"<h1>Indoor Electric Vehicle Telematics Project</h1>\n");
  fprintf (fp,"<script type=\"text/javascript\">\n");
  fprintf (fp,"function refresh() {\n");
  fprintf (fp,"\tsetTimeout(function () {\n");
  fprintf (fp,"\tlocation.reload()\n");
  fprintf (fp,"\t}, 100);\n");
  fprintf (fp,"}\n");
  fprintf (fp,"</script>\n");
  fprintf (fp,"<br> <p>\n");
  
  fprintf (fp,"<head>\n");
  fprintf (fp,"<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"> \n");
  fprintf (fp,"<style>\n");
  fprintf (fp,"<center>\n");
  fprintf (fp,"table, th, td {\n");
  fprintf (fp,"border: 1px solid black;\n");
  fprintf (fp,"border-collapse: collapse;\n");
  fprintf (fp,"}\n");
  fprintf (fp,"th, td {\n");
  fprintf (fp,"padding: 5px;\n");
  fprintf (fp,"text-align: center;\n");
  fprintf (fp,"}\n");
  fprintf (fp,"</style>\n");
  fprintf (fp,"</head>\n");
  fprintf (fp,"<table style=""width:100%"">");
  fprintf (fp,"<tr>");
  fprintf (fp,"<th>Vehicle ID</th>");
  fprintf (fp,"<th>%d</th>", VEHICLEID);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>Handle: </th>");
  fprintf (fp,"<th>%d</th>", handle);
  fprintf (fp,"</tr>\n<tr>");

  fprintf (fp,"<th>Cummulative Wheel Revolutions</th>");
  fprintf (fp,"<th>%d</th>\n</tr>", wheel_revolutions );
  fprintf (fp,"<tr>\n<th>DiffTime w.r.t Last Rev Event</th>");
  fprintf (fp,"<th>%d</th>", mydifftime);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>Num of Wheel Revolutions </th>");
  fprintf (fp,"<th>%d</th>", num_wheel_revolutions);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>Distance </th>");
  fprintf (fp,"<th>%.2f</th>", distance);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>Total Distance </th>");
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>%.2f</th>", total_distance);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>Speed </th>");
  fprintf (fp,"<th>%d</th>", speed);
  fprintf (fp,"</tr>\n<tr>");
#ifdef NEED_CRANKS
  fprintf (fp,"<th>Cummulative Crank Revolutions </th>");
  fprintf (fp,"<th>%d</th>", crank_revolutions);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>CrankTime w.r.t Last Crank Event </th>");
  fprintf (fp,"<th>%d</th>", mycranktime);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>Num of Crank Revolutions </th>");
  fprintf (fp,"<th>%d</th>", crank_revolutions);
  fprintf (fp,"</tr>\n<tr>");
#endif
  fprintf (fp,"<th>Battery Level</th>");
  fprintf (fp,"<th>%d</th>", battery_level);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"</tr>");
  fprintf (fp,"</table>");
  fprintf (fp,"</center>");
  fprintf (fp,"<script>\n refresh();\n </script>\n");
  fprintf (fp, "</body>\n");
  fprintf (fp, "</html>\n");


  fclose(fp);
}

int main(int argc, char *argv[]) {

  int sock;
  struct sockaddr_in server;
  int mysock;
  int rval;
  int optval =1;
  int i;
  uint16_t plen;

  uint8_t* value;
  int k;
  uint16_t bl_len;

#ifdef ZMQ
  void *context = zmq_ctx_new ();
  void * responder = zmq_socket (context, ZMQ_REP);
  int rc = zmq_bind (responder, "tcp://*:5556");
  assert (rc == 0);
#else

  printf (" Creating a socket\n");
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0) {
    perror("Failed to create socket");
    exit(1);
  }
  else printf ("Success\n");

  /* specify the address for the socket */
  
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(5000);
  
#ifdef SETSOCK  /* Hold the address for reuse */
  
  int reuse = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");
  
#ifdef SO_REUSEPORT
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
    perror("setsockopt(SO_REUSEPORT) failed");
#endif
#endif
  
  /* Call bind */
  
  if(bind(sock, (struct sockaddr *)&server, sizeof(server))) {
    perror("bind failed");
    exit(1);
  }
  else printf ("Success\n");
  
  /* Listen */
  listen(sock, 5);
  
  /* Accept */
  
  mysock = accept(sock , (struct sockaddr *) 0, 0);
  
  if(mysock == -1)
    perror("accept failed");
  else printf ("accepted\n");
  
#endif

  while(1) {
    
	memset(pdu, 0, sizeof(pdu));

#ifdef ZMQ
	rval = zmq_recv (responder, pdu, sizeof(pdu), 0);
#else
	rval = recv(mysock, pdu, sizeof(pdu), 0);
#endif
	if ( rval < 0 ) {
	  perror("reading stream message error\n");
	} else {
	  printf("\nPacket recieved from client is: \n");
	  for (i = 0; i < 21; i++) {
	    printf("%02x ",pdu[i]);
	  }
	  printf("\n");
	  printf("\n");
	  memcpy(&diameter,&pdu[0],sizeof(float));
	  printf("Diameter of wheel is: %.2f\n",diameter);
	  vehicle_id = pdu[4];
	  printf("Vehicle Id is: %d\n", vehicle_id);
	  plen = get_le16(&pdu[5]);
	  /* pdu is the data packet which contains following: 
	     ---> First two bytes as Length of the packet.
	     ---> One byte of OPCODE
	     ---> Two Bytes of Handle
	     ---> The remaining bytes are data basing on the handle.
	     Changing code to get values of characteristics basing on handles.
	  */ 
	  /* 
	     Handle we need are:
	     Battery Level : 
	     Handle 			---> 	0x002c
	     Characteristic 	---> 	2A19
	     Speed and Cadence:
	     Handle 			--->	0x0012
	     Characteristic	--->	2A5B
	  */
	  //uint16_t handle;
	  handle = get_le16(&pdu[8]);
	  printf("Handle : 0x%04x \n", handle);
	  /* We can use this Handle to process what data is coming in */
	  /* The following code is assuming the packet arrived has the lenght of the pdu itself */
	  switch(handle) {
	  case BATTERY_LEVEL_HANDLE:
	    bl_len = plen-3;
	    if(bl_len < 0) {
	      perror("Protocol Error... Data is not present in the arrived packet.\n");
	    } else {
	      value = (uint8_t*) malloc(sizeof(uint8_t) * bl_len);
	      for (k = 0; k < bl_len; k++) {
		value[k] = pdu[k+5];
	      }
	    }
	    if(bl_len == 1) {
	      battery_level = value[0];
	    }
	    break;
	    
	  case SPEED_HANDLE:
	    /* 
	       Still this function doesn't handle the following:
	       --> Entry and Exit of vehicle.
	       --> what if mydifftime < 0 or curr_wheel_revolutions < prev_wheel_revolutions?
	       --> Also, doesn't calculate the distance travelled from the starting to exit.
	       --> Total Distance is still cumulative. We need to figure out how to get total distance travelled from Entry to Exit. 
	    */
	    flags = pdu[10];
            printf("Data Received :\t");
	    for (i = 7; i < plen + 5; i++)
	      printf("%02x ",pdu[i]);
	    printf("\n");
	    printf("\t");
	    printf("Length: ");
	    for(i = 5; i < 7; i++){
	      printf("%02x ",pdu[i]);
	    }
	    printf("\n");
	    printf("\tOpcode: %02x\n",pdu[7]);
	    printf("\tHandle: 0x%04x\n",handle);
	    printf("\tFlags : %02x\n",flags);
	    printf("\n");
            printf("\tData from the Handle 0x%04x: ",handle);
            for(i = 10; i < plen+5; i++)
	      printf("%02x ",pdu[i]);
  	    printf("\n");
  	    printf("\n");
	    printf("Processing data from the handle...\n");
	    if(flags & WHEEL_REVOLUTIONS_PRESENT) {
	      wheel_revolutions = get_le32(&pdu[11]);
	      last_wheel_rev_time = get_le16(&pdu[15]);
	      printf("\tNumber of current wheel revolutions:\t%d\n", wheel_revolutions);
	      printf("\tLast wheel revolution event time:\t%d\n", last_wheel_rev_time);
	      mydifftime = (last_wheel_rev_time -  prev_last_wheel_rev_time)/1024;
	      if (mydifftime > 0) {
		num_wheel_revolutions = wheel_revolutions - prev_wheel_revolutions;
		printf("\tTime elaspsed since last wheel event:\t%d secs\n",mydifftime);
		printf ("\tWheel Revolutions during the elapsed time :\t%d\n",num_wheel_revolutions);
		prev_last_wheel_rev_time = last_wheel_rev_time;
		prev_wheel_revolutions = wheel_revolutions;
		distance = (float)(num_wheel_revolutions * 3.14 * diameter);
		speed = (int)((num_wheel_revolutions * 3.14 * diameter) / mydifftime);
		printf ("\t\tDistance travelled during elapsed time :%.2fm \n", distance);
		printf ("\t\tSpeed of the vehicle:\t\t%d m/sec \n", speed);
		if(total_distance == 0.00 ) {
		  total_distance = distance;
		}
		else {
		  total_distance += distance;
		  printf("\tTotal Distance travelled is:\t%.2fm\n\n",total_distance);
		}
	      }
	    }
	    break;
	  case LINK_LOSS:
	    printf("This device supports link loss and we can get data about RSSI.\n");
	    break;
	  default:
	    printf("The handle doesn't contain any required data... \n");
	    break;
	  }
#ifdef NEED_CRANKS
	  
	  //-----> I believe we don't need this. <------ 
	  if(flags & CRANK_REVOLUTIONS_PRESENT) {
	    crank_revolutions = get_le16(&pdu[12]);	\
	    last_crank_rev_time = get_le16(&pdu[14]);
	    
	    printf ("Crank revs: %d crank rev time: %d\n",crank_revolutions,last_crank_rev_time);
	    mycranktime = (last_crank_rev_time - prev_last_crank_rev_time)/1024;
	    if(mycranktime > 0) {
	      num_crank_revolutions = crank_revolutions - prev_crank_revolutions;
	      printf ("%d crank revolutions during the time: %d secs \n", num_crank_revolutions, mycranktime);
	      prev_crank_revolutions = crank_revolutions;
	      prev_last_crank_rev_time = last_crank_rev_time;
	      printf ("Cadence value is %d rpm:\n ",(int)(num_crank_revolutions*60));
	    }
	  }
#endif

#ifdef ZMQ
	  zmq_send (responder, "Yes", 3, 0);
#endif
	  generate_html_page ();
	  
	}
	
  }
  
#ifdef ZMQ
  zmq_close (responder);
  zmq_ctx_destroy (context);
#else
  close (mysock);
#endif
  
  return 0;
}  

