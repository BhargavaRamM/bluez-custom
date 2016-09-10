#include <zmq.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
#include <unistd.h>
//#include <netdb.h>
#include "util.h"

#define WHEEL_REVOLUTIONS_PRESENT	0x01
#define CRANK_REVOLUTIONS_PRESENT	0x02

#define VEHICLEID 1
#define PI	3.14

//unsigned char 	data[15];
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
unsigned char pdu[16];

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
  fprintf (fp,"<th>Cummulative Crank Revolutions </th>");
  fprintf (fp,"<th>%d</th>", crank_revolutions);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>CrankTime w.r.t Last Crank Event </th>");
  fprintf (fp,"<th>%d</th>", mycranktime);
  fprintf (fp,"</tr>\n<tr>");
  fprintf (fp,"<th>Num of Crank Revolutions </th>");
  fprintf (fp,"<th>%d</th>", crank_revolutions);
  fprintf (fp,"</tr>\n<tr>");
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

  int i;
  void *context = zmq_ctx_new ();
  void * responder = zmq_socket (context, ZMQ_REP);
  int rc = zmq_bind (responder, "tcp://*:5556");
  assert (rc == 0);
 
  while(1) {

    memset(pdu, 0, sizeof(pdu));
    zmq_recv (responder, pdu, sizeof(pdu), 0);

   
      printf ("receiving data\n");
      for (i = 2; i < 16; i++) printf("%02x ",pdu[i]);
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
     // battery_level = pdu[14];
        zmq_send (responder, "Yes", 3, 0);
        generate_html_page ();
    }
  
  zmq_close (responder);
  zmq_ctx_destroy (context);
  return 0;
}
