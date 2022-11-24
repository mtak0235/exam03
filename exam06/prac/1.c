#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>

typedef struct s_client {
	int fd;
	int id;
	struct s_client *next;
} t_client;
t_client *g_clients;
int sockfd;
fd_set curr_sock, cpy_read, cpy_write;
char tmp[100000], str[100000], buf[100000];
char msg[1000];
int g_id = 0;
void fatal() {
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sockfd);//
	exit(1);
}

int get_max_fd() {
	t_client *t = g_clients;
	int maxfd = sockfd;
	while (t) {////
		if (t->fd > maxfd)
			maxfd = t->fd;
		t = t->next;
	}
	return maxfd;
}

int add_client_list(int fd) {
	t_client *t = g_clients;
	t_client *n;
	if (!(n = calloc(1, sizeof(t_client))))
		fatal();
	n->fd = fd;
	n->id = g_id++;
	n->next = NULL;
	if (!g_clients)
		g_clients = n;
	else {
		while (t->next) 
			t = t->next;
		t->next = n;
	}
	return n->id;
}
//
void send_all(int fd, char *m) {
	t_client *t = g_clients;
	while (t) {
		if (t->fd != fd && FD_ISSET(t->fd, &cpy_write))
			if (send(t->fd, m, strlen(m), 0) < 0)
				fatal();
		t = t->next;
	}
}
//
void add_client() {
	struct sockaddr_in cliaddr;
	socklen_t len = sizeof(cliaddr);
	int clifd;
	if ((clifd = accept(sockfd, (struct sockaddr *)&cliaddr, &len)) < 0)
		fatal();
	sprintf(msg, "server: client %d just arrived\n", add_client_list(clifd));
	send_all(clifd, msg);
	FD_SET(clifd, &curr_sock);
}

int get_id(int fd) {
	t_client *t = g_clients;
	while (t->next) {
		if (t->fd == fd)
			return t->id;
		t = t->next;
	}
	return -1;
}
int rm_client(int fd) {
	t_client *t =g_clients;
	t_client *del;
	int id = get_id(fd);
	if (t && t->fd == fd) {//
		g_clients = g_clients->next;
		free(t);
	} else {
		while(t && t->next && t->next->fd != fd)//
			t = t->next;
		del = t->next;//
		t->next = t->next->next;
		free(del);
	}
	return id;
}
//tmp, buf
void ex_msg(int fd) {
	int i = 0;
	int j = 0;
	while (str[i]) {
		tmp[j] = str[i];
		j++;
		if (str[i] == '\n') {
			sprintf(buf, "client %d: %s", get_id(fd), tmp);
			send_all(fd, buf);
			j = 0;
			bzero(&tmp, sizeof(tmp));
			bzero(&buf, sizeof(buf));
		}
		i++;
	}
	bzero(&str, strlen(str));
}
int main(int ag, char **av) {
	if (ag != 2) {
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}
	struct sockaddr_in servaddr;
	uint16_t port = atoi(av[1]);
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
	if ((sockfd = socket(AF_INET,SOCK_STREAM, 0)) < 0)
		fatal();
	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		fatal();
	if (listen(sockfd, 10) < 0)
		fatal();
	FD_ZERO(&curr_sock);
	FD_SET(sockfd, &curr_sock);
	bzero(&tmp, sizeof(tmp));
	bzero(&str, sizeof(str));
	bzero(&buf, sizeof(buf));
	while (1) {
		cpy_read = cpy_write = curr_sock;
		if (select(get_max_fd() + 1,&cpy_read, &cpy_write, NULL, NULL) < 0)
			continue;
		for (int fd = 0; fd <= get_max_fd(); fd++) {
			if (FD_ISSET(fd, &curr_sock)) {//
				if (fd == sockfd) {
					bzero(&msg, sizeof(msg));
					add_client();
					break;
				} else {
					int r_recv = 1000;
					while (r_recv == 1000 || str[strlen(str) - 1] != '\n') { //
						r_recv = recv(fd, str + strlen(str), 1000, 0);//
						if (r_recv <= 0)//
							break;
					}
					if (r_recv <= 0) {//
						bzero(&msg, sizeof(msg));
						sprintf(msg, "server: client %d just left\n", rm_client(fd));
						send_all(fd, msg);
						FD_CLR(fd, &curr_sock);
						close(fd);
						break;
					} else {
						ex_msg(fd);
					}
				}
			}
		}
	}
}