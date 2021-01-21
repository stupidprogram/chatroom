#ifndef SERVER_H__
#define SERVER_H__

class Server
{
public:
	Server();
	~Server();
	void run();


private:
	typedef struct
	{
		int fd;
		char name[1];
	} User;
	typedef struct
	{
		int pipe[2];
		User** user;
	} Chatroom;
	int logon(int fd, char* name);
	int connect_to_room(int room,int fd, const char* name);
	void chat(int room);
	Chatroom** chatroom;
};

#endif
