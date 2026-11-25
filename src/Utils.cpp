#include "Utils.hpp"

std::string	Utils::toUpper(const std::string& str)
{
	std::string	ret = str;
	for (size_t i = 0; i < ret.size(); ++i)
		ret[i] = std::toupper(static_cast<unsigned char>(ret[i]));
	return ret;
}

std::string	Utils::toLower(const std::string& str)
{
	std::string	ret = str;
	for (size_t i = 0; i < ret.size(); ++i)
		ret[i] = std::tolower(static_cast<unsigned char>(ret[i]));
	return ret;
}

std::string	Utils::trim(const std::string& str)
{
	size_t	start = 0;
	size_t	end = str.size();

	while (start < end && (str[start] == ' ' || str[start] == '\t'))
		++start;
	while (end >start && (str[end - 1] == ' ' || str[end - 1] == '\t'))
		--end;
	return str.substr(start, end - start);
}