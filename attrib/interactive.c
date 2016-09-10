/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011  Nokia Corporation
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>

#include <glib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "lib/bluetooth.h"
#include "lib/sdp.h"
#include "lib/uuid.h"

#include "src/shared/util.h"
#include "btio/btio.h"
#include "att.h"
#include "gattrib.h"
#include "gatt.h"
#include "gatttool.h"
#include "client/display.h"

#define PORT "3490"
#define MAX_DATA_SIZE 256

#define WHEEL_REV_SUPPORT		0x01
#define CRANK_REV_SUPPORT		0x02

#define WHEEL_REVOLUTIONS_PRESENT	0x01
#define CRANK_REVOLUTIONS_PRESENT	0x02
#define CUMULATIVE_VALUE		0x01

struct csc;

struct cscvalues {
	struct csc	*csc;
	bool		wheel_rev_present;
	uint32_t 	wheel_revolutions;
	uint16_t	last_wheel_rev_time;
	bool 		crank_rev_present;
	uint32_t	crank_revolutions;
	uint16_t	last_crank_rev_time;
};

static GIOChannel *iochannel = NULL;
static GAttrib *attrib = NULL;
static GMainLoop *event_loop;
static GString *prompt;

static char *opt_src = NULL;
static char *opt_dst = NULL;
static char *opt_dst_type = NULL;
static char *opt_sec_level = NULL;
static int opt_psm = 0;
static int opt_mtu = 0;
static int start;
static int end;

static void cmd_help(int argcp, char **argvp);

static enum state {
	STATE_DISCONNECTED,
	STATE_CONNECTING,
	STATE_CONNECTED
} conn_state;

#define error(fmt, arg...) \
	rl_printf(COLOR_RED "Error: " COLOR_OFF fmt, ## arg)

#define failed(fmt, arg...) \
	rl_printf(COLOR_RED "Command Failed: " COLOR_OFF fmt, ## arg)

static char *get_prompt(void)
{
	if (conn_state == STATE_CONNECTED)
		g_string_assign(prompt, COLOR_BLUE);
	else
		g_string_assign(prompt, "");

	if (opt_dst)
		g_string_append_printf(prompt, "[%17s]", opt_dst);
	else
		g_string_append_printf(prompt, "[%17s]", "");

	if (conn_state == STATE_CONNECTED)
		g_string_append(prompt, COLOR_OFF);

	if (opt_psm)
		g_string_append(prompt, "[BR]");
	else
		g_string_append(prompt, "[LE]");

	g_string_append(prompt, "> ");

	return prompt->str;
}


static void set_state(enum state st)
{
	conn_state = st;
	rl_set_prompt(get_prompt());
}

uint32_t 	wheel_revolutions;
uint16_t	last_wheel_rev_time;
uint32_t 	prev_wheel_revolutions;
uint16_t	prev_last_wheel_rev_time;
uint16_t	mydifftime;
uint32_t        num_wheel_revolutions;
uint32_t 	num_crank_revolutions;
uint32_t	crank_revolutions;
uint32_t	last_crank_rev_time;
uint32_t	prev_crank_revolutions;
uint32_t	prev_last_crank_rev_time;
uint16_t 	mycranktime;


int sock;
struct sockaddr_in server;
struct hostent *host;
unsigned char buff[16];


static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data)
{
	uint8_t *opdu;
	uint16_t handle, i, olen, k,data_len;
	size_t plen;
	GString *s;
	uint8_t *req_data;
	int *data;
	handle = get_le16(&pdu[1]);
	data_len = len-3;
	struct cscvalues val;
	uint8_t flags;


	switch (pdu[0]) {
	case ATT_OP_HANDLE_NOTIFY:
		s = g_string_new(NULL);
		g_string_printf(s, "Notification handle = 0x%04x value at handle is : ",
									handle);
		buff[0]  = (len &0xFF);
		buff[1] = (len >> 8) & 0x00FF;
		for (i = 2; i < len; i++){
		  buff[i] = pdu[i];
		  g_print ("%02x ",buff[i]);
		}


		break;
	case ATT_OP_HANDLE_IND:
		s = g_string_new(NULL);
		g_string_printf(s, "Indication   handle = 0x%04x value: ",
									handle);
		break;
	default:
		error("Invalid opcode\n");
		return;
	}

	for (i = 3; i < len; i++) {
		g_string_append_printf(s, "%02x ", pdu[i]);
	}
	/*
	flags = pdu[3];
	if(flags & WHEEL_REVOLUTIONS_PRESENT) {
	  wheel_revolutions = get_le32(&pdu[4]);
	  last_wheel_rev_time = get_le16(&pdu[8]);

	  g_print("wrevs: %d wrevtime: %d\n", wheel_revolutions,last_wheel_rev_time);

	  mydifftime = (last_wheel_rev_time -  prev_last_wheel_rev_time)/1024;
	  if (mydifftime > 0) {
	    num_wheel_revolutions = wheel_revolutions - prev_wheel_revolutions;
	    g_print ("%d Wheel Revolutions during the time : %d secs\n",num_wheel_revolutions, mydifftime);
	    prev_last_wheel_rev_time = last_wheel_rev_time;
	    prev_wheel_revolutions = wheel_revolutions;
	    //Calculate speed and Distance
	    g_print("Distance in %d time is %d \n", mydifftime, (int)(num_wheel_revolutions * 3.14 * 0.508));
	    g_print("Speed in last %d secs is %d m/sec \n", mydifftime, (int)((num_wheel_revolutions * 3.14 * 0.508) / mydifftime));
	  }
	}
	if(flags & CRANK_REVOLUTIONS_PRESENT) {
		crank_revolutions = get_le16(&pdu[10]);\
		last_crank_rev_time = get_le16(&pdu[12]);

		g_print("Crank revs: %d crank rev time: %d\n",crank_revolutions,last_crank_rev_time);
		mycranktime = (last_crank_rev_time - prev_last_crank_rev_time)/1024;
		if(mycranktime > 0) {
			num_crank_revolutions = crank_revolutions - prev_crank_revolutions;
			g_print("%d crank revolutions during the time: %d secs \n", num_crank_revolutions, mycranktime);
			prev_crank_revolutions = crank_revolutions;
			prev_last_crank_rev_time = last_crank_rev_time;
			g_print("Cadence value is %drpm:\n ",(int)(num_crank_revolutions*60));
		}
	}

*/
	/*
	req_data = (uint8_t*) malloc(sizeof(uint8_t) * (len-2));
	req_data[0] = len-3;
        for(i = 1; i < len - 2; i++) {
                req_data[i] = pdu[i+2];
        }
	data = (int* )malloc(sizeof(int) * len);
	for(i = 0; i < len; i++) {
		data[i] = pdu[i];
	}
	flags = *data;

	data++;
	data_len--;
	memset(&val, 0, sizeof (val));

	if(flags & WHEEL_REVOLUTIONS_PRESENT) {
		if(data_len < 6) {
			error("Wheel revolutions data missing.");
			exit(1);
		}
		printf("You are in Wheel rev... \n");
		val.wheel_rev_present = true;
		val.wheel_revolutions = get_le32(data);
		g_print("Wheel revolutions: %02x\n",val.wheel_revolutions);
		val.last_wheel_rev_time = get_le16(data + 4);
                g_print("last wheel revolution time: %02x\n",val.last_wheel_rev_time);
		data += 6;
		data_len -= 6;
		val.wheel_revolutions = 0;
		val.last_wheel_rev_time = 0;
		g_print("Wheel Revs: %02x\n", val.wheel_revolutions);
	}	

	if(flags & CRANK_REVOLUTIONS_PRESENT) {
		if(data_len < 4) {
			error("Crank Revolution data missing...");
			exit(1);
		}
		val.crank_rev_present = true;
		val.crank_revolutions = get_le16(data);
                g_print("crank revolutions: %02x\n",val.crank_revolutions);
		val.last_crank_rev_time = get_le16(data + 2);
                g_print("last crank revolution time: %02x\n",val.last_crank_rev_time);
		data = data + 4;
		data_len = data_len - 4;
	}
	*/
//	printf("Values of wheel and crank revolutions are %02x and %02x:", val.wheel_revolutions, val.crank_revolutions);

/*
	for(i = 0; i < len - 2; i++) {
		g_print("required_data is : %02x	",req_data[i]);
	}

	p_data = (int*) malloc(sizeof(int*) * (len-2));
	for(i = 0; i< len - 2; i++) {
		p_data[i] = (int) req_data[i];
		printf("Normal value is :%d", p_data[i]);
	}
*/
	/*
	for (i = 0; i < len; i++){
	  buff[i] = pdu[i];
	  g_print ("%02x ",buff[i]);
	}
	*/
	    
        if(send(sock, buff, sizeof(buff),0) < 0) {
                perror("Sending data failed...");
                exit(1);
        }
	else g_print ("Data sent\n");

	//        close(sock);
//	free(req_data);
//	free(val);
//	free(data);

	rl_printf("%s\n", s->str);
	g_string_free(s, TRUE);

	if (pdu[0] == ATT_OP_HANDLE_NOTIFY)
		return;

	opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_confirmation(opdu, plen);

	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, NULL);
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
	uint16_t mtu;
	uint16_t cid;

	if (err) {
		set_state(STATE_DISCONNECTED);
		error("%s\n", err->message);
		return;
	}

	bt_io_get(io, &err, BT_IO_OPT_IMTU, &mtu,
				BT_IO_OPT_CID, &cid, BT_IO_OPT_INVALID);

	if (err) {
		g_printerr("Can't detect MTU, using default: %s", err->message);
		g_error_free(err);
		mtu = ATT_DEFAULT_LE_MTU;
	}

	if (cid == ATT_CID)
		mtu = ATT_DEFAULT_LE_MTU;

	attrib = g_attrib_new(iochannel, mtu, false);
	g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
						events_handler, attrib, NULL);
	g_attrib_register(attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
						events_handler, attrib, NULL);
	set_state(STATE_CONNECTED);
	rl_printf("Connection successful\n");
}

static void disconnect_io()
{
	if (conn_state == STATE_DISCONNECTED)
		return;

	g_attrib_unref(attrib);
	attrib = NULL;
	opt_mtu = 0;

	g_io_channel_shutdown(iochannel, FALSE, NULL);
	g_io_channel_unref(iochannel);
	iochannel = NULL;

	set_state(STATE_DISCONNECTED);
}

static void primary_all_cb(uint8_t status, GSList *services, void *user_data)
{
	GSList *l;

	if (status) {
		error("Discover all primary services failed: %s\n",
						att_ecode2str(status));
		return;
	}

	if (services == NULL) {
		error("No primary service found\n");
		return;
	}

	for (l = services; l; l = l->next) {
		struct gatt_primary *prim = l->data;
		rl_printf("attr handle: 0x%04x, end grp handle: 0x%04x uuid: %s\n",
				prim->range.start, prim->range.end, prim->uuid);
	}
}

static void primary_by_uuid_cb(uint8_t status, GSList *ranges, void *user_data)
{
	GSList *l;

	if (status) {
		error("Discover primary services by UUID failed: %s\n",
							att_ecode2str(status));
		return;
	}

	if (ranges == NULL) {
		error("No service UUID found\n");
		return;
	}

	for (l = ranges; l; l = l->next) {
		struct att_range *range = l->data;
		rl_printf("Starting handle: 0x%04x Ending handle: 0x%04x\n",
						range->start, range->end);
	}
}

static void included_cb(uint8_t status, GSList *includes, void *user_data)
{
	GSList *l;

	if (status) {
		error("Find included services failed: %s\n",
							att_ecode2str(status));
		return;
	}

	if (includes == NULL) {
		rl_printf("No included services found for this range\n");
		return;
	}

	for (l = includes; l; l = l->next) {
		struct gatt_included *incl = l->data;
		rl_printf("handle: 0x%04x, start handle: 0x%04x, "
					"end handle: 0x%04x uuid: %s\n",
					incl->handle, incl->range.start,
					incl->range.end, incl->uuid);
	}
}

static void char_cb(uint8_t status, GSList *characteristics, void *user_data)
{
	GSList *l;

	if (status) {
		error("Discover all characteristics failed: %s\n",
							att_ecode2str(status));
		return;
	}

	for (l = characteristics; l; l = l->next) {
		struct gatt_char *chars = l->data;

		rl_printf("handle: 0x%04x, char properties: 0x%02x, char value "
				"handle: 0x%04x, uuid: %s\n", chars->handle,
				chars->properties, chars->value_handle,
				chars->uuid);
	}
}

static void char_desc_cb(uint8_t status, GSList *descriptors, void *user_data)
{
	GSList *l;

	if (status) {
		error("Discover descriptors failed: %s\n",
							att_ecode2str(status));
		return;
	}

	for (l = descriptors; l; l = l->next) {
		struct gatt_desc *desc = l->data;

		rl_printf("handle: 0x%04x, uuid: %s\n", desc->handle,
								desc->uuid);
	}
}

static void char_read_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	uint8_t value[plen];
	ssize_t vlen;
	int i;
	GString *s;

	if (status != 0) {
		error("Characteristic value/descriptor read failed: %s\n",
							att_ecode2str(status));
		return;
	}

	vlen = dec_read_resp(pdu, plen, value, sizeof(value));
	if (vlen < 0) {
		error("Protocol error\n");
		return;
	}

	s = g_string_new("Characteristic value/descriptor: ");
	for (i = 0; i < vlen; i++)
		g_string_append_printf(s, "%02x ", value[i]);

	rl_printf("%s\n", s->str);
	g_string_free(s, TRUE);
}

static void char_read_by_uuid_cb(guint8 status, const guint8 *pdu,
					guint16 plen, gpointer user_data)
{
	struct att_data_list *list;
	int i;
	GString *s;

	if (status != 0) {
		error("Read characteristics by UUID failed: %s\n",
							att_ecode2str(status));
		return;
	}

	list = dec_read_by_type_resp(pdu, plen);
	if (list == NULL)
		return;

	s = g_string_new(NULL);
	for (i = 0; i < list->num; i++) {
		uint8_t *value = list->data[i];
		int j;

		g_string_printf(s, "handle: 0x%04x \t value: ",
							get_le16(value));
		value += 2;
		for (j = 0; j < list->len - 2; j++, value++)
			g_string_append_printf(s, "%02x ", *value);

		rl_printf("%s\n", s->str);
	}

	att_data_list_free(list);
	g_string_free(s, TRUE);
}

static void cmd_exit(int argcp, char **argvp)
{
	rl_callback_handler_remove();
	g_main_loop_quit(event_loop);
}

static gboolean channel_watcher(GIOChannel *chan, GIOCondition cond,
				gpointer user_data)
{
	disconnect_io();

	return FALSE;
}

static void cmd_connect(int argcp, char **argvp)
{
	GError *gerr = NULL;

	if (conn_state != STATE_DISCONNECTED)
		return;

	if (argcp > 1) {
		g_free(opt_dst);
		opt_dst = g_strdup(argvp[1]);

		g_free(opt_dst_type);
		if (argcp > 2)
			opt_dst_type = g_strdup(argvp[2]);
		else
			opt_dst_type = g_strdup("public");
	}

	if (opt_dst == NULL) {
		error("Remote Bluetooth address required\n");
		return;
	}

	rl_printf("Attempting to connect to %s\n", opt_dst);
	set_state(STATE_CONNECTING);
	iochannel = gatt_connect(opt_src, opt_dst, opt_dst_type, opt_sec_level,
					opt_psm, opt_mtu, connect_cb, &gerr);
	if (iochannel == NULL) {
		set_state(STATE_DISCONNECTED);
		error("%s\n", gerr->message);
		g_error_free(gerr);
	} else
		g_io_add_watch(iochannel, G_IO_HUP, channel_watcher, NULL);

	// client socket inittiation
	sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock < 0) {
                perror("Socket failed...");
                exit(1);
        }

        server.sin_family = AF_INET;
        host = gethostbyname("localhost");

        if(host == 0) {
                perror("Get host name failed...");
                exit(1);
        }
        memcpy(&server.sin_addr, host->h_addr, host->h_length);
        server.sin_port = htons(5000);
        if(connect(sock, (struct sockaddr*) &server, sizeof(server)) < 0) {
                perror("Connection failed...");
                exit(1);
	}
	else printf (" Connection succesfull\n");

}

static void cmd_disconnect(int argcp, char **argvp)
{
	disconnect_io();
}

static void cmd_primary(int argcp, char **argvp)
{
	bt_uuid_t uuid;

	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (argcp == 1) {
		gatt_discover_primary(attrib, NULL, primary_all_cb, NULL);
		return;
	}

	if (bt_string_to_uuid(&uuid, argvp[1]) < 0) {
		error("Invalid UUID\n");
		return;
	}

	gatt_discover_primary(attrib, &uuid, primary_by_uuid_cb, NULL);
}

static int strtohandle(const char *src)
{
	char *e;
	int dst;

	errno = 0;
	dst = strtoll(src, &e, 16);
	if (errno != 0 || *e != '\0')
		return -EINVAL;

	return dst;
}

static void cmd_included(int argcp, char **argvp)
{
	int start = 0x0001;
	int end = 0xffff;

	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (argcp > 1) {
		start = strtohandle(argvp[1]);
		if (start < 0) {
			error("Invalid start handle: %s\n", argvp[1]);
			return;
		}
		end = start;
	}

	if (argcp > 2) {
		end = strtohandle(argvp[2]);
		if (end < 0) {
			error("Invalid end handle: %s\n", argvp[2]);
			return;
		}
	}

	gatt_find_included(attrib, start, end, included_cb, NULL);
}

static void cmd_char(int argcp, char **argvp)
{
	int start = 0x0001;
	int end = 0xffff;

	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (argcp > 1) {
		start = strtohandle(argvp[1]);
		if (start < 0) {
			error("Invalid start handle: %s\n", argvp[1]);
			return;
		}
	}

	if (argcp > 2) {
		end = strtohandle(argvp[2]);
		if (end < 0) {
			error("Invalid end handle: %s\n", argvp[2]);
			return;
		}
	}

	if (argcp > 3) {
		bt_uuid_t uuid;

		if (bt_string_to_uuid(&uuid, argvp[3]) < 0) {
			error("Invalid UUID\n");
			return;
		}

		gatt_discover_char(attrib, start, end, &uuid, char_cb, NULL);
		return;
	}

	gatt_discover_char(attrib, start, end, NULL, char_cb, NULL);
}



static void cmd_char_desc(int argcp, char **argvp)
{
	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (argcp > 1) {
		start = strtohandle(argvp[1]);
		if (start < 0) {
			error("Invalid start handle: %s\n", argvp[1]);
			return;
		}
	} else
		start = 0x0001;

	if (argcp > 2) {
		end = strtohandle(argvp[2]);
		if (end < 0) {
			error("Invalid end handle: %s\n", argvp[2]);
			return;
		}
	} else
		end = 0xffff;

	gatt_discover_desc(attrib, start, end, NULL, char_desc_cb, NULL);
}

static void cmd_read_hnd(int argcp, char **argvp)
{
	int handle;

	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (argcp < 2) {
		error("Missing argument: handle\n");
		return;
	}

	handle = strtohandle(argvp[1]);
	if (handle < 0) {
		error("Invalid handle: %s\n", argvp[1]);
		return;
	}

	gatt_read_char(attrib, handle, char_read_cb, attrib);
}

static void cmd_read_uuid(int argcp, char **argvp)
{
	int start = 0x0001;
	int end = 0xffff;
	bt_uuid_t uuid;

	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (argcp < 2) {
		error("Missing argument: UUID\n");
		return;
	}

	if (bt_string_to_uuid(&uuid, argvp[1]) < 0) {
		error("Invalid UUID\n");
		return;
	}

	if (argcp > 2) {
		start = strtohandle(argvp[2]);
		if (start < 0) {
			error("Invalid start handle: %s\n", argvp[1]);
			return;
		}
	}

	if (argcp > 3) {
		end = strtohandle(argvp[3]);
		if (end < 0) {
			error("Invalid end handle: %s\n", argvp[2]);
			return;
		}
	}

	gatt_read_char_by_uuid(attrib, start, end, &uuid, char_read_by_uuid_cb,
									NULL);
}

static void char_write_req_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	if (status != 0) {
		error("Characteristic Write Request failed: "
						"%s\n", att_ecode2str(status));
		return;
	}

	if (!dec_write_resp(pdu, plen) && !dec_exec_write_resp(pdu, plen)) {
		error("Protocol error\n");
		return;
	}

	rl_printf("Characteristic value was written successfully\n");
}

static void cmd_char_write(int argcp, char **argvp)
{
	uint8_t *value;
	size_t plen;
	int handle;

	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (argcp < 3) {
		rl_printf("Usage: %s <handle> <new value>\n", argvp[0]);
		return;
	}

	handle = strtohandle(argvp[1]);
	if (handle <= 0) {
		error("A valid handle is required\n");
		return;
	}

	plen = gatt_attr_data_from_string(argvp[2], &value);
	if (plen == 0) {
		error("Invalid value\n");
		return;
	}

	if (g_strcmp0("char-write-req", argvp[0]) == 0)
		gatt_write_char(attrib, handle, value, plen,
					char_write_req_cb, NULL);
	else
		gatt_write_cmd(attrib, handle, value, plen, NULL, NULL);

	g_free(value);
}

static void cmd_sec_level(int argcp, char **argvp)
{
	GError *gerr = NULL;
	BtIOSecLevel sec_level;

	if (argcp < 2) {
		rl_printf("sec-level: %s\n", opt_sec_level);
		return;
	}

	if (strcasecmp(argvp[1], "medium") == 0)
		sec_level = BT_IO_SEC_MEDIUM;
	else if (strcasecmp(argvp[1], "high") == 0)
		sec_level = BT_IO_SEC_HIGH;
	else if (strcasecmp(argvp[1], "low") == 0)
		sec_level = BT_IO_SEC_LOW;
	else {
		rl_printf("Allowed values: low | medium | high\n");
		return;
	}

	g_free(opt_sec_level);
	opt_sec_level = g_strdup(argvp[1]);

	if (conn_state != STATE_CONNECTED)
		return;

	if (opt_psm) {
		rl_printf("Change will take effect on reconnection\n");
		return;
	}

	bt_io_set(iochannel, &gerr,
			BT_IO_OPT_SEC_LEVEL, sec_level,
			BT_IO_OPT_INVALID);
	if (gerr) {
		error("%s\n", gerr->message);
		g_error_free(gerr);
	}
}

static void exchange_mtu_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	uint16_t mtu;

	if (status != 0) {
		error("Exchange MTU Request failed: %s\n",
						att_ecode2str(status));
		return;
	}

	if (!dec_mtu_resp(pdu, plen, &mtu)) {
		error("Protocol error\n");
		return;
	}

	mtu = MIN(mtu, opt_mtu);
	/* Set new value for MTU in client */
	if (g_attrib_set_mtu(attrib, mtu))
		rl_printf("MTU was exchanged successfully: %d\n", mtu);
	else
		error("Error exchanging MTU\n");
}

static void cmd_mtu(int argcp, char **argvp)
{
	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (opt_psm) {
		failed("Operation is only available for LE transport.\n");
		return;
	}

	if (argcp < 2) {
		rl_printf("Usage: mtu <value>\n");
		return;
	}

	if (opt_mtu) {
		failed("MTU exchange can only occur once per connection.\n");
		return;
	}

	errno = 0;
	opt_mtu = strtoll(argvp[1], NULL, 0);
	if (errno != 0 || opt_mtu < ATT_DEFAULT_LE_MTU) {
		error("Invalid value. Minimum MTU size is %d\n",
							ATT_DEFAULT_LE_MTU);
		return;
	}

	gatt_exchange_mtu(attrib, opt_mtu, exchange_mtu_cb, NULL);
}

static struct {
	const char *cmd;
	void (*func)(int argcp, char **argvp);
	const char *params;
	const char *desc;
} commands[] = {
	{ "help",		cmd_help,	"",
		"Show this help"},
	{ "exit",		cmd_exit,	"",
		"Exit interactive mode" },
	{ "quit",		cmd_exit,	"",
		"Exit interactive mode" },
	{ "connect",		cmd_connect,	"[address [address type]]",
		"Connect to a remote device" },
	{ "disconnect",		cmd_disconnect,	"",
		"Disconnect from a remote device" },
	{ "primary",		cmd_primary,	"[UUID]",
		"Primary Service Discovery" },
	{ "included",		cmd_included,	"[start hnd [end hnd]]",
		"Find Included Services" },
	{ "characteristics",	cmd_char,	"[start hnd [end hnd [UUID]]]",
		"Characteristics Discovery" },
	{ "char-desc",		cmd_char_desc,	"[start hnd] [end hnd]",
		"Characteristics Descriptor Discovery" },
	{ "char-read-hnd",	cmd_read_hnd,	"<handle>",
		"Characteristics Value/Descriptor Read by handle" },
	{ "char-read-uuid",	cmd_read_uuid,	"<UUID> [start hnd] [end hnd]",
		"Characteristics Value/Descriptor Read by UUID" },
	{ "char-write-req",	cmd_char_write,	"<handle> <new value>",
		"Characteristic Value Write (Write Request)" },
	{ "char-write-cmd",	cmd_char_write,	"<handle> <new value>",
		"Characteristic Value Write (No response)" },
	{ "sec-level",		cmd_sec_level,	"[low | medium | high]",
		"Set security level. Default: low" },
	{ "mtu",		cmd_mtu,	"<value>",
		"Exchange MTU for GATT/ATT" },
	{ NULL, NULL, NULL}
};

static void cmd_help(int argcp, char **argvp)
{
	int i;

	for (i = 0; commands[i].cmd; i++)
		rl_printf("%-15s %-30s %s\n", commands[i].cmd,
				commands[i].params, commands[i].desc);
}

static void parse_line(char *line_read)
{
	char **argvp;
	int argcp;
	int i;

	if (line_read == NULL) {
		rl_printf("\n");
		cmd_exit(0, NULL);
		return;
	}

	line_read = g_strstrip(line_read);

	if (*line_read == '\0')
		goto done;

	add_history(line_read);

	if (g_shell_parse_argv(line_read, &argcp, &argvp, NULL) == FALSE)
		goto done;

	for (i = 0; commands[i].cmd; i++)
		if (strcasecmp(commands[i].cmd, argvp[0]) == 0)
			break;

	if (commands[i].cmd)
		commands[i].func(argcp, argvp);
	else
		error("%s: command not found\n", argvp[0]);

	g_strfreev(argvp);

done:
	free(line_read);
}

static gboolean prompt_read(GIOChannel *chan, GIOCondition cond,
							gpointer user_data)
{
	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		g_io_channel_unref(chan);
		return FALSE;
	}

	rl_callback_read_char();

	return TRUE;
}

static char *completion_generator(const char *text, int state)
{
	static int index = 0, len = 0;
	const char *cmd = NULL;

	if (state == 0) {
		index = 0;
		len = strlen(text);
	}

	while ((cmd = commands[index].cmd) != NULL) {
		index++;
		if (strncmp(cmd, text, len) == 0)
			return strdup(cmd);
	}

	return NULL;
}

static char **commands_completion(const char *text, int start, int end)
{
	if (start == 0)
		return rl_completion_matches(text, &completion_generator);
	else
		return NULL;
}

static guint setup_standard_input(void)
{
	GIOChannel *channel;
	guint source;

	channel = g_io_channel_unix_new(fileno(stdin));

	source = g_io_add_watch(channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				prompt_read, NULL);

	g_io_channel_unref(channel);

	return source;
}

static gboolean signal_handler(GIOChannel *channel, GIOCondition condition,
							gpointer user_data)
{
	static unsigned int __terminated = 0;
	struct signalfd_siginfo si;
	ssize_t result;
	int fd;

	if (condition & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		g_main_loop_quit(event_loop);
		return FALSE;
	}

	fd = g_io_channel_unix_get_fd(channel);

	result = read(fd, &si, sizeof(si));
	if (result != sizeof(si))
		return FALSE;

	switch (si.ssi_signo) {
	case SIGINT:
		rl_replace_line("", 0);
		rl_crlf();
		rl_on_new_line();
		rl_redisplay();
		break;
	case SIGTERM:
		if (__terminated == 0) {
			rl_replace_line("", 0);
			rl_crlf();
			g_main_loop_quit(event_loop);
		}

		__terminated = 1;
		break;
	}

	return TRUE;
}

static guint setup_signalfd(void)
{
	GIOChannel *channel;
	guint source;
	sigset_t mask;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
		perror("Failed to set signal mask");
		return 0;
	}

	fd = signalfd(-1, &mask, 0);
	if (fd < 0) {
		perror("Failed to create signal descriptor");
		return 0;
	}

	channel = g_io_channel_unix_new(fd);

	g_io_channel_set_close_on_unref(channel, TRUE);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	source = g_io_add_watch(channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				signal_handler, NULL);

	g_io_channel_unref(channel);

	return source;
}

int interactive(const char *src, const char *dst,
		const char *dst_type, int psm)
{
	guint input;
	guint signal;

	opt_sec_level = g_strdup("low");

	opt_src = g_strdup(src);
	opt_dst = g_strdup(dst);
	opt_dst_type = g_strdup(dst_type);
	opt_psm = psm;

	prompt = g_string_new(NULL);

	event_loop = g_main_loop_new(NULL, FALSE);

	input = setup_standard_input();
	signal = setup_signalfd();

	rl_attempted_completion_function = commands_completion;
	rl_erase_empty_line = 1;
	rl_callback_handler_install(get_prompt(), parse_line);

	g_main_loop_run(event_loop);

	rl_callback_handler_remove();
	cmd_disconnect(0, NULL);
	g_source_remove(input);
	g_source_remove(signal);
	g_main_loop_unref(event_loop);
	g_string_free(prompt, TRUE);

	g_free(opt_src);
	g_free(opt_dst);
	g_free(opt_sec_level);

	return 0;
}
