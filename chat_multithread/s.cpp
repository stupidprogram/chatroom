#include <iostream>

#include "Server.hpp"

int main()
{
	Server* my_server = new Server();
	my_server->run();
	return 0;
}
