#ifndef SERVER_H__
#define SERVER_H__

class Chatroom;

class Server
{
public:
	Server();
	~Server();
	void run();


private:
	int logon(int fd, char* name);
	int connect_to_room(int room,int fd, const char* name);
	void chat(int room);
	Chatroom** chatroom;

	int user_add(int room, int fd, const char* name);
};

#endif
