#include "UriUtils.hpp"

#include <iostream>

void	testPE(const std::string& input)
{
	bool	result = UriUtils::validatePercentEncoding(input);

	std::cout << "input: " << input
				<< " | success: " << result << std::endl;
}


void testDecode(const std::string& input, std::string& output)
{
	bool result = UriUtils::percentDecode(input, output);

	std::cout
		<< "decode: " << input
		<< " | result: " << output
		<< " | success: " << result << std::endl;
}

void testDecodedPath(const std::string& output)
{
	bool result = UriUtils::validateDecodedPath(output);

	std::cout
		<< "decoded path: " << output
		<< " | success: " << result << std::endl;
}

void testNormalize(const std::string& input, std::string& output)
{
	bool result = UriUtils::normalizePath(input, output);

	std::cout
		<< " | result: " << output
		<< " | success: " << result
		<< std::endl;
}

int	main(void)
{
	// //test step by step
	// const std::string input = "//a/%2e%2e/%2e%2e/etc";

	// std::string decoded;
	// std::string output;

	// testPE(input);
	// testDecode(input, decoded);
	// testDecodedPath(decoded);
	// testNormalize(decoded, output);


	// overall test for req.rawTarget
	Request req;
	req.rawTarget = "//a/%2e%2e/b?name=hello%20world";

	bool result = UriUtils::handleRawTarget(req);

	std::cout << result << std::endl;
	std::cout << "req.path= " << req.path << std::endl;
	std::cout << "req.query= " << req.query << std::endl;
	
	return 0;
}