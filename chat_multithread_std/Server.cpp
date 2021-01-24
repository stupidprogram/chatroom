#include <memory>
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
#include <thread>
#include <mutex>
#include <unordered_map>

#include "Server.hpp"
#include "Protocal.hpp"

std::mutex mut;

class Chatroom
{
public:
	int efd;
	std::unordered_map<int, char*> user; 
};


Server::Server()
{
	chatroom = reinterpret_cast<Chatroom**>(malloc(sizeof(Chatroom*) * ROOMMAX));
	memset(chatroom, 0, sizeof(Chatroom*) * ROOMMAX);
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
		chatroom[room] = new Chatroom();
		chatroom[room]->efd = epoll_create(USERMAX);
		user_add(room, fd, name);
		std::thread t_chat(&Server::chat, this, room);
		t_chat.detach();
	}
	return 0;
}

void Server::chat(int room)
{
	struct epoll_event my_envent;
	char buff[BUFSIZ];
	char msg_title[BUFSIZ];
	while (1)
	{
		int ret = epoll_wait(chatroom[room]->efd, &my_envent, 1, -1);

		if (ret <= 0)
		{
			perror("epoll_wait():");
			continue;
		}

		printf("Thread id is %ld\n", std::this_thread::get_id());
		int len;
		len = read(my_envent.data.fd, buff, BUFSIZ);
		mut.lock();
		if (len == 0)
		{
			close(my_envent.data.fd);
			free(chatroom[room]->user[my_envent.data.fd]);
			chatroom[room]->user.erase(my_envent.data.fd);
			epoll_ctl(chatroom[room]->efd, EPOLL_CTL_DEL, my_envent.data.fd, &my_envent);
		}
		else
		{
			sprintf(msg_title, "Message from %s :", chatroom[room]->user[my_envent.data.fd]);
			for (auto it = chatroom[room]->user.begin(); it != chatroom[room]->user.end(); ++it)
			{
				if (it->first != my_envent.data.fd)
				{
					write(it->first, msg_title, strlen(msg_title));
					write(it->first, buff, len);
				}
			}
		}
		mut.unlock();
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


int Server::user_add(int room, int fd, const char* name)
{
	mut.lock();
	if (chatroom[room]->user.size() < USERMAX)
	{
		if (chatroom[room]->user.find(fd) == chatroom[room]->user.end())
		{
			chatroom[room]->user[fd] = reinterpret_cast<char*>(malloc(strlen(name)));
			strcpy(chatroom[room]->user[fd], name);
			struct epoll_event my_envent;
			my_envent.data.fd = fd;
			my_envent.events = EPOLLIN;
			epoll_ctl(chatroom[room]->efd, EPOLL_CTL_ADD, fd, &my_envent);
		}
	}
	mut.unlock();
	return 1;
}



































