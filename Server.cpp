#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>

#include "Server.hpp"
#include "Protocal.hpp"

static const int CONTROL_LEN = CMSG_LEN(sizeof(int));

static int send_fd(int fd, int fd_to_send, const char* name)
{
	struct iovec my_iovec[1];
	struct msghdr my_msghdr;
	char buff[0];

	my_iovec[0].iov_base = buff;
	my_iovec[0].iov_len = 1;
	my_msghdr.msg_iov = my_iovec;
	my_msghdr.msg_iovlen = 1;
	my_msghdr.msg_name = NULL;
	my_msghdr.msg_namelen = 0;

	struct cmsghdr* my_cm;
	my_cm = reinterpret_cast<struct cmsghdr*>(malloc(CONTROL_LEN));
	my_cm->cmsg_level = SOL_SOCKET;
	my_cm->cmsg_type = SCM_RIGHTS;
	my_cm->cmsg_len = CONTROL_LEN;
	my_msghdr.msg_control = my_cm;
	my_msghdr.msg_controllen = CONTROL_LEN;
	*(int *)CMSG_DATA(my_cm) = fd_to_send;
	sendmsg(fd, &my_msghdr, 0);
	free(my_cm);
	write(fd, name, strlen(name));
	return 0;
}

static int recv_fd(int fd, char* name)
{
	struct iovec my_iovec[1];
	struct msghdr my_msghdr;
	char buff[0];
	int newfd;

	my_iovec[0].iov_base = buff;
	my_iovec[0].iov_len = 1;
	my_msghdr.msg_iov = my_iovec;
	my_msghdr.msg_iovlen = 1;
	my_msghdr.msg_name = NULL;
	my_msghdr.msg_namelen = 0;

	struct cmsghdr* my_cm;
	my_cm = reinterpret_cast<struct cmsghdr*>(malloc(CONTROL_LEN));
	my_msghdr.msg_control = my_cm;
	my_msghdr.msg_controllen = CONTROL_LEN;
	recvmsg(fd, &my_msghdr, 0);
	read(fd, name, NAMEMAX);
	newfd = *(int*)CMSG_DATA(my_cm);
	free(my_cm);
	return newfd;
}

Server::Server()
{
	chatroom = reinterpret_cast<Chatroom**>(malloc(sizeof(Chatroom*) * ROOMMAX));

	for (int i = 0; i < ROOMMAX; ++i)
	{
		chatroom[i] = NULL;
	}
}


int Server::logon(int fd, char* name)
{
	char buff[BUFSIZ];
	struct log_post* rpost;
	int size = sizeof(struct log_post) + NAMEMAX - 1;
	rpost = reinterpret_cast<struct log_post*>(malloc(size));

	bzero(rpost, size);

	int len = read(fd, rpost, size);

	strcpy(name, rpost->name);

	return ntohl(rpost->room);
}


int Server::connect_to_room(int room, int fd, const char* name)
{
	if (chatroom[room] != NULL)
	{
		send_fd(chatroom[room]->pipe[0], fd, name);
	}
	else
	{
		pid_t pid;
		int pipefd[2];

		int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);

		if (ret < 0)
		{
			perror("socketpair():");
			return -1;
		}
		chatroom[room] = reinterpret_cast<Chatroom*>(malloc(sizeof(Chatroom)));
		
		pid = fork();

		if (pid < 0)
		{
			perror("fork():");
			return -1;
		}
		else if (pid == 0)
		{
			chatroom[room]->pipe[0] = pipefd[0];
			chatroom[room]->pipe[1] = pipefd[1];
			//chatroom[room]->user = new User*[USERMAX];
			pid = fork();
			if (pid < 0)
			{
				perror("fork():");
				return -1;
			}
			if (pid == 0)
			{
				chat(room);
			}
			else
				exit(0);
		}
		else
		{
			chatroom[room]->pipe[0] = pipefd[0];
			chatroom[room]->pipe[1] = pipefd[1];
			wait(NULL);
			send_fd(chatroom[room]->pipe[0], fd, name);
		}
	}
	return 0;
}

void Server::chat(int room)
{

	chatroom[room]->user = reinterpret_cast<User**>(malloc(sizeof(User*) * USERMAX));
	for (int i = 0; i < USERMAX; ++i)
	{
		chatroom[room]->user[i] = NULL;
	}
	int efd;
	struct epoll_event my_envent;
	char buff[BUFSIZ];

	efd = epoll_create(USERMAX);

	my_envent.data.fd = chatroom[room]->pipe[1];
	my_envent.events = EPOLLIN;

	epoll_ctl(efd, EPOLL_CTL_ADD, chatroom[room]->pipe[1], &my_envent);

	while (1)
	{
		int ret = epoll_wait(efd, &my_envent, 1, -1);

		if (ret <= 0)
		{
			perror("epoll_wait():");
			continue;
		}

		if (chatroom[room]->pipe[1] == my_envent.data.fd)
		{
			bool flag = true;
			for (int i = 0; i < USERMAX; ++i)
			{
				if (chatroom[room]->user[i] == NULL)
				{
					int size = sizeof(User) + NAMEMAX - 1;
					chatroom[room]->user[i] = reinterpret_cast<User*>(malloc(size));
					chatroom[room]->user[i]->fd = recv_fd(chatroom[room]->pipe[1], chatroom[room]->user[i]->name);
					printf("Chatroom[%d] : The new connected user is %s\n", room, chatroom[room]->user[i]->name);
					fflush(NULL);
					my_envent.data.fd = chatroom[room]->user[i]->fd;
					my_envent.events = EPOLLIN;
					epoll_ctl(efd, EPOLL_CTL_ADD, chatroom[room]->user[i]->fd, &my_envent);
					flag = false;
					break;
				}
			}
			if (flag)
			{
				fprintf(stderr, "out 1 range!");
				continue;
			}
		}
		else
		{
			char msg_title[BUFSIZ];
			int len;
			len = read(my_envent.data.fd, buff, BUFSIZ);
			for (int i = 0; i < USERMAX; ++i)
			{
				if (chatroom[room]->user[i] != NULL && chatroom[room]->user[i]->fd == my_envent.data.fd)
				{
					sprintf(msg_title, "Message from %s :", chatroom[room]->user[i]->name);
					break;
				}
			}

			for (int i = 0; i < USERMAX; ++i)
			{
				if (chatroom[room]->user[i] != NULL && chatroom[room]->user[i]->fd != my_envent.data.fd)
				{
					write(chatroom[room]->user[i]->fd, msg_title, strlen(msg_title));
					write(chatroom[room]->user[i]->fd, buff, len);
				}
			}

		}
	}
}


void Server::run()
{
	int ld, rd;
	struct sockaddr_in laddr, raddr;
	socklen_t raddr_len;

	ld = socket(AF_INET, SOCK_STREAM, 0);

	laddr.sin_family = AF_INET;
	laddr.sin_port = htons(SERVPORT);
	laddr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(ld, reinterpret_cast<struct sockaddr*>(&laddr), sizeof(laddr));
	
	listen(ld, 128);
	raddr_len = 0;

	while (1)
	{
		rd = accept(ld, reinterpret_cast<struct sockaddr*>(&raddr), &raddr_len);
		int ret;
		char name[BUFSIZ];
		ret = logon(rd, name);
		if (ret < 0)
		{
			close(rd);
			continue;
		}
		ret = connect_to_room(ret, rd, name);

		if (ret < 0)
		{
			close(rd);
			continue;
		}

		close(rd);

	}
}



































