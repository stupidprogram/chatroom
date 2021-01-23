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
#include <pthread.h>

#include "Server.hpp"
#include "Protocal.hpp"

struct pthread_arg
{
	void* ptr;
	int room;
};

static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

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
		user_add(room, fd, name);
	}
	else
	{
		chatroom[room] = reinterpret_cast<Chatroom*>(malloc(sizeof(Chatroom)));
		chatroom[room]->efd = epoll_create(USERMAX);
		chatroom[room]->user = reinterpret_cast<User**>(malloc(sizeof(User*) * USERMAX));
		for (int i = 0; i < USERMAX; ++i)
		{
			chatroom[room]->user[i] = NULL;
		}
		pthread_t ptid;
		struct pthread_arg* cur_arg;
		cur_arg = reinterpret_cast<struct pthread_arg*>(malloc(sizeof(struct pthread_arg)));
		cur_arg->ptr = this;
		cur_arg->room = room;
		user_add(room, fd, name);
		pthread_create(&ptid, NULL, thread_run<Server, &Server::chat>, cur_arg);
	}
	return 0;
}

void Server::chat(int room)
{
	struct epoll_event my_envent;
	char buff[BUFSIZ];
	while (1)
	{
		int ret = epoll_wait(chatroom[room]->efd, &my_envent, 1, -1);

		if (ret <= 0)
		{
			perror("epoll_wait():");
			continue;
		}

		
		pthread_mutex_lock(&mut);
		printf("Thread id is %ld\n", pthread_self());
		char msg_title[BUFSIZ];
		int len;
		len = read(my_envent.data.fd, buff, BUFSIZ);
		if (len == 0)
		{
			for (int i = 0; i < USERMAX; ++i)
			{
				if (chatroom[room]->user[i] != NULL && chatroom[room]->user[i]->fd == my_envent.data.fd)
				{
					close(chatroom[room]->user[i]->fd);
					free(chatroom[room]->user[i]);
					chatroom[room]->user[i] = NULL;
					epoll_ctl(chatroom[room]->efd, EPOLL_CTL_DEL, my_envent.data.fd, &my_envent);
					break;
				}
			}
		}
		else
		{
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
		pthread_mutex_unlock(&mut);
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
	}
}


template<typename TYPE, void (TYPE::*chat)(int room)>
void* Server::thread_run(void *arg)
{
	struct pthread_arg* cur_arg;
	cur_arg = reinterpret_cast<struct pthread_arg*>(arg);
	TYPE* cur_ptr = reinterpret_cast<TYPE*>(cur_arg->ptr);
	cur_ptr->chat(cur_arg->room);
	return NULL;
}

int Server::user_add(int room, int fd, const char* name)
{
	bool flag = true;
	for (int i = 0; i < USERMAX; ++i)
	{
		if (chatroom[room]->user[i] == NULL)
		{
			pthread_mutex_lock(&mut);
			int size = sizeof(User) + NAMEMAX - 1;
			chatroom[room]->user[i] = reinterpret_cast<User*>(malloc(size));
			chatroom[room]->user[i]->fd = fd;
			strcpy(chatroom[room]->user[i]->name, name);
			struct epoll_event my_envent;
			my_envent.data.fd = fd;
			my_envent.events = EPOLLIN;
			epoll_ctl(chatroom[room]->efd, EPOLL_CTL_ADD, fd, &my_envent);
			flag = false;
			pthread_mutex_unlock(&mut);
			break;
		}
	}
	if (flag)
	{
		fprintf(stderr, "out range!");
		return -1;
	}
	return 1;
}



































