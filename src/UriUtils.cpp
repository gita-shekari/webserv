#include "UriUtils.hpp"

#include <cctype>

#include <vector>

bool	UriUtils::handleRawTarget(Request& req)
{
	std::string	rawPath;
	std::string	rawQuery;

	size_t	qMark = req.rawTarget.find('?');
	if (qMark == std::string::npos)
		rawPath = req.rawTarget;
	else
	{
		rawPath = req.rawTarget.substr(0, qMark);
		rawQuery = req.rawTarget.substr(qMark + 1);
	}

	//validate percent-encoding syntax in both path and query
	if (!validatePercentEncoding(rawPath))
		return false;
	if (!validatePercentEncoding(rawQuery))
		return false;

	//decode only the path
	std::string decodedPath;

	if (!percentDecode(rawPath, decodedPath))
		return false;

	//validate the decoded path
	if (!validateDecodedPath(decodedPath))
		return false;

	//normalize ".", "..", repeated slashed, etc
	std::string normalizedPath;

	if (!normalizePath(decodedPath, normalizedPath))
		return false;

	req.path = normalizedPath;
	req.query = rawQuery;

	return true;
}

//are all '%'s followed by 2 valid hex digit?
bool	UriUtils::validatePercentEncoding(const std::string& str)
{
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (str[i] != '%')
			continue;
		if (i + 2 >= str.size())
			return false;
		if (!std::isxdigit(static_cast<unsigned char>(str[i + 1]))
			|| !std::isxdigit(static_cast<unsigned char>(str[i + 2])))
			return false;
		i += 2;
	}
	return true;
}

bool	UriUtils::percentDecode(const std::string& input, std::string& output)
{
	output.clear();

	for (size_t i = 0; i < input.size(); ++i)
	{
		if (input[i] != '%')
		{
			output += input[i];
			continue;
		}

		if (i + 2 >= input.size())
			return false;

		int high = hexValue(input[i + 1]);
		int low = hexValue(input[i + 2]);

		if (high == -1 || low == -1)
			return false;

		char decoded = static_cast<char>(high * 16 + low);

		if (decoded == '/' || decoded == '\\')
			return false;
		output += decoded;
		i += 2;
	}
	return true;
}


bool	UriUtils::validateDecodedPath(const std::string& path)
{
	if (path.empty())
		return false;

	if (path[0] != '/')
		return false;

	for (size_t i = 0; i < path.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(path[i]);

		if (c == '\0')
			return false;

		if (c < 32 || c == 127)
			return false;
	}
	return true;
}

bool	UriUtils::normalizePath(const std::string& input, std::string& output)
{
	output.clear();

	std::vector<std::string> segments;

	size_t	i = 0;
	size_t	size = input.size();

	bool hadTrailingSlash = (size > 1 && input[size - 1] == '/');

	while (i < size)
	{
		while (i < size && input[i] == '/')
			++i;
		if (i >= size)
			break;
		size_t	start = i;
		while (i < size && input[i] != '/')
			++i;
		std::string segment = input.substr(start, i - start);
		if (segment == ".") //ignore
			continue;
		if (segment == "..") // to previous segment
		{
			if (segments.empty())
				return false;
			segments.pop_back();
			continue;
		}
		segments.push_back(segment);
	}
	output = "/";

	for (size_t j = 0; j < segments.size(); ++j)
	{
		if (j > 0)
			output += "/";
		output += segments[j];
	}
	if (hadTrailingSlash && output != "/")
		output += "/";
	return true;
}

int	UriUtils::hexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}
