#include <sys/stat.h>
#include <string>
#include <iostream>
#include <cctype>
#include <limits>

std::string	toLower(const std::string& str)
{
	std::string	ret = str;
	for (size_t i = 0; i < ret.size(); ++i)
		ret[i] = std::tolower(static_cast<unsigned char>(ret[i]));
	return ret;
}

std::string	trim(const std::string& str)
{
	size_t	start = 0;
	size_t	end = str.size();

	while (start < end && (str[start] == ' ' || str[start] == '\t'))
		++start;
	while (end >start && (str[end - 1] == ' ' || str[end - 1] == '\t'))
		--end;
	return str.substr(start, end - start);
}
bool needToClose(std::string& value)
{
	size_t comma = 0;
	size_t start = 0;
	std::string token;
	while (start < value.size())
	{
		comma = value.find(',', start);
		if (comma == std::string::npos)
			comma = value.size();

		token = toLower(trim(value.substr(start, comma - start)));
		if (token == "close")
			return true;
		start = comma + 1;
	}
	return false;
}

int main(void)
{

	std::string value = "";
	if (needToClose(value))
	{
		std::cout << "contain close" << std::endl;
	}
	else
	{
		std::cout << "didn't indentify close" << std::endl;
	}
}