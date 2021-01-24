#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <strings.h>
#include <sys/epoll.h>

#include "Client.hpp"
#include "Protocal.hpp"

Client::Client()
{
	name = new char[BUFSIZ];
	printf("Please tell your name:\n");
	scanf("%s", name);
	printf("Which room you want in:\n");
	scanf("%d", &room);
}


void Client::run()
{
	struct log_post* my_post;
	int ld, rd;
	struct sockaddr_in raddr, laddr;
	socklen_t raddr_len;
	char buff[BUFSIZ];
	
	int size = sizeof(struct log_post) + strlen(name) - 1;

	my_post = reinterpret_cast<struct log_post*>(malloc(size));

	bzero(my_post, size);

	my_post->room = htonl(room);
	strcpy(my_post->name, name);

	ld = socket(AF_INET, SOCK_STREAM, 0);

	raddr.sin_family = AF_INET;
	raddr.sin_port = htons(SERVPORT);
	inet_pton(AF_INET, SERVADDR, &raddr.sin_addr.s_addr);

	int ret = connect(ld, reinterpret_cast<struct sockaddr*>(&raddr), sizeof(raddr));
	if (ret < 0)
	{
		perror("connect():");
		exit(1);
	}

	write(ld, my_post, size);

	struct epoll_event my_event;

	int efd = epoll_create(3);

	my_event.data.fd = STDIN_FILENO;
	my_event.events = EPOLLIN;

	epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, &my_event);

	my_event.data.fd = ld;
	my_event.events = EPOLLIN;

	epoll_ctl(efd, EPOLL_CTL_ADD, ld, &my_event);

	while (1)
	{
		ret = epoll_wait(efd, &my_event, 1, -1);

		if (ret < 0)
		{
			fprintf(stderr, "epoll_wait():");
			continue;
		}

		if (my_event.data.fd == ld)
		{
			int len = read(ld, buff, BUFSIZ);
			write(STDOUT_FILENO, buff, len);
		}
		else
		{
			int len = read(STDIN_FILENO, buff, BUFSIZ);
			write(ld, buff, len);
		}
	}

}


































