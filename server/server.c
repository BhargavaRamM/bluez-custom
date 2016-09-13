//#include <zmq.h>
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

#define BATTERY_LEVEL_HANDLE 		0x002c
#define SPEED_HANDLE			0x0012
#define LINK_LOSS			0x1803 // UUID. We don't know the exact handle. 

#define WHEEL_REVOLUTIONS_PRESENT	0x01
#define CRANK_REVOLUTIONS_PRESENT	0x02

#define VEHICLEID 	1
#define PI		3.14

//unsigned char 	data[15];
uint32_t 	wheel_revolutions;
uint16_t	last_wheel_rev_time;
uint32_t 	prev_wheel_revolutions;
uint16_t	mydifftime;
uint32_t        num_wheel_revolutions;
uint32_t	prev_last_wheel_rev_time;
uint8_t         flags;
float 		distance;
uint32_t 	speed;
float		total_distance;
uint8_t		battery_level;
unsigned char 	pdu[16];
uint16_t 	handle;

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
  fprintf (fp,"\t}, 1000);\n");
  fprintf (fp,"}\n");
  fprintf (fp,"</script>\n");
  fprintf (fp,"<br> <p>\n");  
  fprintf (fp,"<head>\n");
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
  fprintf (fp,"<th>%04x</th>", handle);
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
/*
  fprintf (fp,"<th>Cummulative Crank Revolutions </th>");
  fprintf (fp,"<th>%d</th>", crank_revolutions);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>CrankTime w.r.t Last Crank Event </th>");
  fprintf (fp,"<th>%d</th>", mycranktime);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>Num of Crank Revolutions </th>");
  fprintf (fp,"<th>%d</th>", crank_revolutions);
  fprintf (fp,"</tr>\n<tr>");
  */
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

  int i,k;
  void *context = zmq_ctx_new ();
  void * responder = zmq_socket (context, ZMQ_REP);
  int rc = zmq_bind (responder, "tcp://\*:5556");
  assert (rc == 0);
  uint16_t plen;
  uint16_t bl_len;
  uint8_t* value;
  while(1) {
	memset(pdu, 0, sizeof(pdu));
	zmq_recv (responder, pdu, sizeof(pdu), 0);

	printf ("Data Received at the server...\n");
	for (i = 0; i < 16; i++) {
		printf("%02x ",pdu[i]);
	}
	printf("\n");
	plen = get_le16(&pdu[0]);
	for (i = 2; i < plen; i++) 
		printf("%02x ",pdu[i]);
	printf("\n");
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
	handle = get_le16(&pdu[3]);
	printf("Handle we are accessing data from is :%04x \n", handle);
	/* We can use this Handle to process what data is coming in */
	/* The following code is assuming the packet arrived has the lenght of the pdu itself */
	switch(handle) {
		case BATTERY_LEVEL_HANDLE:
	//		int k;
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
			flags = pdu[5];
			if(flags & WHEEL_REVOLUTIONS_PRESENT) {
				wheel_revolutions = get_le32(&pdu[6]);
				last_wheel_rev_time = get_le16(&pdu[10]);
				printf("Number of current wheel revolutions: %d\nLast wheel revolution event time: %d\n", wheel_revolutions,last_wheel_rev_time);
				mydifftime = (last_wheel_rev_time -  prev_last_wheel_rev_time)/1024;
				if (mydifftime > 0) {
					num_wheel_revolutions = wheel_revolutions - prev_wheel_revolutions;
					printf ("%d Wheel Revolutions during the time : %d secs\n",num_wheel_revolutions, mydifftime);
					prev_last_wheel_rev_time = last_wheel_rev_time;
					prev_wheel_revolutions = wheel_revolutions;
					//Calculate speed and Distance
					distance = (float)(num_wheel_revolutions * 3.14 * 0.508);
					speed = (int)((num_wheel_revolutions * 3.14 * 0.508) / mydifftime);
					printf ("Distance travelled in last %d secs is %f \n", mydifftime, distance);
					printf ("Speed in last %d secs is %d m/sec \n", mydifftime, speed);
					if(total_distance == 0.00 ) {
						total_distance = distance;
					}
					else {
						total_distance += distance;
						printf("Total Distance travelled is: %f\n",total_distance);
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
	/* ------> I believe we don't need this. <------ 
	if(flags & CRANK_REVOLUTIONS_PRESENT) {
		crank_revolutions = get_le16(&pdu[12]);		\
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
	*/
     // battery_level = pdu[14];
        zmq_send (responder, "Yes", 3, 0);
        generate_html_page ();
 }
  
  zmq_close (responder);
  zmq_ctx_destroy (context);
  return 0;
}
