#ifndef pouque_irc_h_included
#define pouque_irc_h_included

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <netdb.h>

#include <sys/select.h>
#include <sys/time.h>

struct pouque_irc
{
	int sock_fd;
};

struct pouque_irc *pouque_irc_create(void);
void pouque_irc_free(struct pouque_irc *p);

int pouque_irc_connect(struct pouque_irc *p, const char *host, int port);
int pouque_irc_disconnect(struct pouque_irc *p);

int pouque_irc_wait(struct pouque_irc *p, int secs);

int pouque_irc_send(struct pouque_irc *p,
	const char *cmd, const char *fmt, ...);

/* implementation */
struct pouque_irc *
pouque_irc_create(void)
{
	struct pouque_irc *p;

	p = malloc(sizeof(struct pouque_irc));
	p->sock_fd = -1;

	return p;
}

void
pouque_irc_free(struct pouque_irc *p)
{
	free(p);
}

int
pouque_irc_connect(struct pouque_irc *p, const char *host, int port)
{
	struct sockaddr_in servaddr;
	struct hostent *he;

	p->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (p->sock_fd == -1) {
		return 0;
	}
	memset(&servaddr, 0, sizeof(servaddr));

	he = gethostbyname(host);
	if (he == NULL) {
		return 0;
	}

	servaddr.sin_family = AF_INET;
	memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);
	/*servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");*/
	servaddr.sin_port = htons(port);

	if (connect(p->sock_fd,
			(struct sockaddr *)&servaddr,
			sizeof(servaddr)) != 0) {

		return 0;
	}

	return 1;
}

int
pouque_irc_disconnect(struct pouque_irc *p)
{
	int rc;

	if (p->sock_fd == -1) {
		return 1;
	}

	rc = close(p->sock_fd);
	if (rc == 0) {
		return 1;
	}

	return 0;
}

int
pouque_irc_wait(struct pouque_irc *p, int secs)
{
	fd_set fds;
	struct timeval tv;
	int rc;

	tv.tv_sec = secs;
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(p->sock_fd, &fds);

	rc = select(p->sock_fd + 1, &fds, NULL, NULL, &tv);

	if (rc == -1) {
		return 0;
	}
	if (rc == 0) {
		/* timeout */
		return 0;
	}

	return 1;
}

int
pouque_irc_send(struct pouque_irc *p, const char *cmd, const char *fmt, ...)
{
	char buf[1024], fmt_buf[1024];
	va_list args;
	ssize_t res;
	size_t sent = 0, buf_size;

	va_start(args, fmt);
	vsnprintf(fmt_buf, sizeof(fmt_buf), fmt, args);
	va_end(args);

	snprintf(buf, sizeof(buf), "%s %s\r\n", cmd, fmt_buf);

	buf_size = strlen(buf);

	while (sent < buf_size) {
		res = write(p->sock_fd, buf + sent, buf_size - sent);
		if (res < 0) {
			return 0;
		}
		sent += res;
	}

	return 1;
}


#endif

