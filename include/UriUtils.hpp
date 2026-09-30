#pragma once

# include "Http.hpp"

# include <string>

namespace UriUtils
{
	//splits, validates, decodes and normalizes req.rawTarget
	bool	handleRawTarget(Request& req);

	//processes
	bool	validatePercentEncoding(const std::string& str);
	bool	percentDecode(const std::string& input, std::string& output);
	bool	validateDecodedPath(const std::string& path);
	bool	normalizePath(const std::string& input, std::string& output);

	//helpers
	static	int hexValue(char c);
}