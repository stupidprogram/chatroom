#ifndef CLIENT_H__
#define CLIENT_H__


class Client
{
public:
	Client();
	void run();


private:
	char* name;
	int room;
	int logon();
};

#endif
