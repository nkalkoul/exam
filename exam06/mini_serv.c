#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

int sprintf(char *, const char *, ...);

typedef struct s_client { int id; char *buf; } t_client;

t_client clients[1024];
fd_set active, rset, wset;
int maxfd, nextid;

void fatal() { write(2, "Fatal error\n", 12); exit(1); }

/* ========== COPIER-COLLER 100% depuis main.c ========== */
int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}
/* ====================================================== */

void send_all(int ex, char *s) {
	for (int i = 0; i <= maxfd; i++)
		if (FD_ISSET(i, &wset) && i != ex)
			send(i, s, strlen(s), 0);
}

int main(int ac, char **av) {
	int s, i, r, c, e;
	struct sockaddr_in a;
	char b[120000], m[120000], *l;

	if (ac != 2) { write(2, "Wrong number of arguments\n", 26); exit(1); }
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) fatal();
	maxfd = s; FD_ZERO(&active); FD_SET(s, &active);

	/* ========== COPIER-COLLER 100% depuis main.c ========== */
	bzero(&a, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	a.sin_port = htons(atoi(av[1]));
	/* ====================================================== */

	if (bind(s, (const struct sockaddr *)&a, sizeof(a))) fatal();
	if (listen(s, 10)) fatal();

	while (1) {
		rset = wset = active;
		if (select(maxfd + 1, &rset, &wset, 0, 0) < 0) continue;
		for (i = 0; i <= maxfd; i++) {
			if (!FD_ISSET(i, &rset)) continue;
			if (i == s) {
				if ((c = accept(s, 0, 0)) < 0) continue;
				if (c > maxfd) maxfd = c;
				clients[c].id = nextid++; clients[c].buf = 0; FD_SET(c, &active);
				sprintf(m, "server: client %d just arrived\n", clients[c].id);
				send_all(c, m); break;
			}
			if ((r = recv(i, b, 119999, 0)) <= 0) {
				sprintf(m, "server: client %d just left\n", clients[i].id);
				send_all(i, m); free(clients[i].buf);
				FD_CLR(i, &active); close(i); break;
			}
			b[r] = 0;
			if (!(clients[i].buf = str_join(clients[i].buf, b))) fatal();
			while ((e = extract_message(&clients[i].buf, &l)) > 0) {
				sprintf(m, "client %d: ", clients[i].id);
				send_all(i, m); send_all(i, l); free(l);
			}
			if (e == -1) fatal();
		}
	}
}
