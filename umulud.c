#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <pthread.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <time.h>

//#define DEBUG

unsigned int debug_level = 0;

#include "usblcd.h"

#define SOCK_PATH "/tmp/umulud"

#define VOLUP	1
#define VOLDOWN	2
#define F1	3
#define F2	4
#define F3	5
#define F4	6
#define F5	7
#define LEFT	8
#define RIGHT	9
#define UP	10
#define	DOWN	11
#define	CENTER	12

struct blink_info {
	char led;
	int time;
};

struct {
	char led[6];
	char light;
} status;

static time_t last;


/* for connecting and communication with the device */
struct usblcd mylcd;

static void do_key(int);
static void* blink(void *);
static void* showtext(void *);
static enum cmds servercmd(FILE *, const char *);
static void* server(void *);
static void info(char, char *);
static void* handle_conn(void *);


static void* blink(void *arg)
{
	struct blink_info *info = (struct blink_info *) arg;
	while(1) {
		usblcd_setled(&mylcd, info->led - F1 + 1, 1);
		usleep(info->time * 1000);
		usblcd_setled(&mylcd, info->led - F1 + 1, 0);
		usleep(info->time * 1000);
	}
	return NULL;
}

static void* showtext(void *arg)
{
	while(1) {
		if(time(NULL) - last > 30) {
			status.light = 0;
			usblcd_backlight(&mylcd, 0);
			usblcd_clear(&mylcd);
		} else {
			char txt[21];
			info(0, txt);
			usblcd_settext(&mylcd, 0, 0, txt);
			info(1, txt);
			usblcd_settext(&mylcd, 1, 0, txt);
		}

		sleep(1);
	}
	return NULL;
}

static void info(char line, char *txt)
{
	size_t readed;
	char b2[8] = "";
	FILE *t2 = fopen("/sys/devices/platform/coretemp.0/temp2_input", "r");
	readed = fread(b2, 1, sizeof(b2), t2);
	fclose(t2);
	if(!readed)
		return;

	if(line == 0) {
		char b3[8] = "";
		FILE *t3 = fopen("/sys/devices/platform/coretemp.0/temp3_input", "r");
		readed = fread(b3, 1, sizeof(b3), t3);
		fclose(t3);
		if(!readed)
			return;

		int temp = (atoi(b2) + atoi(b3)) / 2000;

		time_t nowt = time(NULL);
		struct tm *now = localtime(&nowt);

		sprintf(txt, "%dC %02d/%02d/%02d %02d%c%02d", temp,
			now->tm_mday, now->tm_mon + 1, now->tm_year + 1900,
			now->tm_hour, now->tm_sec & 1 ? ':' : ' ' ,now->tm_min);

	} else if(line == 1) {
		float loadavg = -1;
		FILE *proc = fopen("/proc/loadavg", "r");
		readed = fscanf(proc, "%f", &loadavg);
		fclose(proc);
		if(!readed)
			return;

		struct statvfs root;
		statvfs("/", &root);

		sprintf(txt, "CPU:%3d%% HDD: %4luGB", (int)(loadavg * 100), root.f_bavail * root.f_bsize / 0x40000000UL);
	}

//	printf("%s\n", outtext);
}

#define LINE 80

enum cmds {
	HI	= 200,
	HELP	= 201,
	QUIT	= 202,
	STATUS	= 203,
	KEY	= 204,
	LIGHT	= 205,
	INFO	= 206,
	ERR	= 500
};

static enum cmds servercmd(FILE * fd, const char *buffer)
{
	enum cmds ret;

	if(!strcmp(buffer, "quit")) {
		fprintf(fd, "%d see you\n", QUIT);
		ret = QUIT;
	} else if(!strcmp(buffer, "status")) {
		int i;
		fprintf(fd, "%d status:\n", STATUS);
		for(i = 0; i < sizeof(status.led) / sizeof(*status.led); i++)
			fprintf(fd, "led%d	%d\n", i, status.led[i]);
		fprintf(fd, "light	%d\n", status.light);
		ret = STATUS;
	} else if(!strncmp(buffer, "key ", 4)) {
		if(strlen(buffer) != 5) {
			fprintf(fd, "%d syntax error\n", ERR);
			ret = ERR;
		}
		char key = buffer[4] - '0';
		if(key < 0 || key > 5) {
			fprintf(fd, "%d invalid key %d\n", ERR, key);
			ret = ERR;
		}
		do_key(key + F1 - 1);
		fprintf(fd, "%d key %d pressed\n", KEY, key);
		ret = KEY;
	} else if(!strcmp(buffer, "info")) {
		fprintf(fd, "%d info:\n", INFO);
		char txt[21];
		ret = INFO;		
		info(0, txt);
		fprintf(fd, "%s\n", txt);
		info(1, txt);
		fprintf(fd, "%s\n", txt);
		ret = INFO;
	} else {
		if(strcmp(buffer, "help"))
			fprintf(fd, "%d invalid command \"%s\"\n", ERR, buffer);
		else
			fprintf(fd, "%d usage:\n", HELP);
		fprintf(fd,	"help	show this help\n"
				"status	show the display status\n"
				"key X	press key X\n"
				"info	show sisyem info\n"
				"light	bring light on\n"
				"quit	close connection\n");
		ret = HELP;
	}

	fflush(fd);
	return ret;
}

static void* handle_conn(void *arg)
{
	int sock = (int)(long)arg;
	int quit = 0;

	FILE *fd = fdopen(sock, "w");
	setvbuf(stdin, (char*)NULL, _IONBF, 0);

//	printf("accept()\n");

	fprintf(fd, "%d uMulud\n", HI);
	fflush(fd);

	while(!quit) {
		char buffer[LINE + 1] = "";

//		scanf(fd, "%s\n", buffer);
//		read(buffer, 1, LINE, fd);

		size_t len = recv(sock, buffer, LINE, 0);

		if(len <= 0)
			break;

		if(buffer[len - 1] == '\n' && len > 1) {
			buffer[len - 1] = 0;

			if(servercmd(fd, buffer) == QUIT)
				quit = 1;
		}
	}

	fclose(fd);
	return NULL;
}

static void* server(void *arg)
{
	int sockfd, newsockfd;
	struct sockaddr_un serv_addr, cli_addr;
	socklen_t clilen;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		exit(1);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, SOCK_PATH);
	unlink(SOCK_PATH);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR on binding");
		exit(1);
	}
	listen(sockfd, 5);
	chmod(SOCK_PATH, 0666);

	while(1) {
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (newsockfd < 0) {
			perror("ERROR on accept");
			exit(1);
		}

		pthread_t conn_t;
		pthread_create(&conn_t, NULL, handle_conn, (void*)(long)newsockfd);
	}

	close(sockfd);

	return NULL;
}

static void do_key(int key)
{
	switch(key) {
	case	F1 ... F5: {
		pthread_t blinkt;
		char cmd[16];

		pthread_create(&blinkt, NULL, blink, &(struct blink_info){key, 250});

		sprintf(cmd, "keydown %d", key);
		int ret = system(cmd);

		pthread_cancel(blinkt);

		status.led[key - F1 + 1] = !!ret;
		usblcd_setled(&mylcd, key - F1 + 1, !!ret);
	}
		break;
	case VOLUP ... VOLDOWN:
		status.light = 1;
		usblcd_backlight(&mylcd, 1);
		last = time(NULL);
		break;
	}
}

int main (int argc, char **argv)
{
	/* for keypad and infrared events */
	struct usblcd_event event;
	last = time(NULL);

#ifdef RC5
	rc5decoder *rc5;
#endif

	/* init hid device and struct usblcd structure */
	//mylcd = new_struct usblcd();
	memset(&mylcd, 0, sizeof(mylcd));

	/* init the USB LCD */
	usblcd_init(&mylcd);
	/* turn off backlight */
	usblcd_backlight(&mylcd, 0);
	/* clear the LCD screen */
	usblcd_clear(&mylcd);
	/* clear all leds status */
	usblcd_setled(&mylcd, 0, 0);
	/* set default cursor */
	usblcd_set_cursor(&mylcd, 0);
	/* set default non blink cursor */
	usblcd_set_cursor_blink(&mylcd, 0);

#ifdef RC5
	rc5 = rc5_init();
#endif

	pthread_t textt;
	pthread_create(&textt, NULL, showtext, NULL);

	memset(&status, 0, sizeof(status));

	pthread_t servert;
	pthread_create(&servert, NULL, server, NULL);

	while (1) {
		if (!usblcd_read_events(&mylcd, &event))
			continue;

		if (event.type == 0 && event.data[0]) {
			int key = event.data[0];
			printf("%d\n", key);
			do_key(key);
		}

#ifdef RC5
		if (event.type == 1)
		    rc5_decode(rc5, event.data, event.length);
#endif
	}

	pthread_cancel(textt);

	usblcd_close(&mylcd);

	return 0;
}
