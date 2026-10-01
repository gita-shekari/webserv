#include <sys/stat.h>
#include <string>
#include <iostream>

int main(void)
{
	// path is the joined path of root + request.path
	std::string path = "test_files/mvp/";
	struct stat buf;
	if (stat(path.c_str(), &buf) == 0)
	{
		if (S_ISDIR(buf.st_mode))
		{
			std::cout << "is direc\n";
		}
		else
		{
			std::cout << "not direc\n";
		}
	}
	else
	{
		std::cerr << "error\n";
	}
}