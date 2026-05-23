#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

// ========== FONCTIONS DONNEES DANS LE SUJET (extract_message + str_join) ==========
// Ces 2 fonctions sont fournies dans main.c le jour de l'exam.
// Tu les copies telles quelles depuis le sujet.

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
			newbuf = calloc(1, strlen(*buf + i + 1) + 1);
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
	newbuf = malloc(len + strlen(add) + 1);
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

// ========== TON CODE (ce que tu dois ecrire de memoire) ==========

int		ids[65536];    // id de chaque client (indexe par fd)
char	*bufs[65536];  // buffer de chaque client (indexe par fd)
int		next_id = 0;   // prochain id a attribuer
fd_set	reads, writes, active;
int		maxfd;
char	tmp[42 + 65536]; // buffer temporaire pour sprintf + messages

void fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void send_all(int except, char *msg)
{
	for (int fd = 0; fd <= maxfd; fd++)
		if (FD_ISSET(fd, &active) && fd != except)
				send(fd, msg, strlen(msg), MSG_NOSIGNAL);
}

int main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	// --- SETUP SERVEUR ---
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		fatal();

	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
	addr.sin_port = htons(atoi(av[1]));

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		fatal();
	if (listen(sockfd, 128) < 0)
		fatal();

	FD_ZERO(&active);
	FD_SET(sockfd, &active);
	maxfd = sockfd;

	// --- BOUCLE PRINCIPALE ---
	while (1)
	{
		reads = writes = active;
		if (select(maxfd + 1, &reads, &writes, 0, 0) < 0)
			fatal();

		for (int fd = 0; fd <= maxfd; fd++)
		{
			if (!FD_ISSET(fd, &reads))
				continue;

			// --- NOUVEAU CLIENT ---
			if (fd == sockfd)
			{
				int client = accept(sockfd, 0, 0);
				if (client < 0)
					fatal();
				ids[client] = next_id++;
				bufs[client] = NULL;
				FD_SET(client, &active);
				if (client > maxfd)
					maxfd = client;
				sprintf(tmp, "server: client %d just arrived\n", ids[client]);
				send_all(client, tmp);
			}
			// --- MESSAGE OU DECONNEXION ---
			else
			{
				char buf[65536 + 1];
				int n = recv(fd, buf, 65536, 0);

				if (n <= 0) // deconnexion
				{
					sprintf(tmp, "server: client %d just left\n", ids[fd]);
					send_all(fd, tmp);
					free(bufs[fd]);
					bufs[fd] = NULL;
					FD_CLR(fd, &active);
					close(fd);
				}
				else // message recu
				{
					buf[n] = 0;
					bufs[fd] = str_join(bufs[fd], buf);
					if (!bufs[fd])
						fatal();
					char *msg;
					while (extract_message(&bufs[fd], &msg) == 1)
					{
						sprintf(tmp, "client %d: ", ids[fd]);
						send_all(fd, tmp);
						send_all(fd, msg);
						free(msg);
					}
				}
			}
		}
	}
}
