#ifndef PROTOCAL_H__
#define PROTOCAL_H__


#define NAMEMAX 40 
#define ROOMMAX 1024
#define USERMAX 1024
#define SERVPORT 9999
#define SERVADDR "127.0.0.1"

struct log_post
{
	int room;
	char name[1];
};


#endif
