#include "Client.hpp"
#include "Config.hpp"

#include <cassert>
#include <iostream>

int main()
{
	ServerConfig serverConfig;

	serverConfig.client_max_body_size = 1000;

	LocationConfig root;
	root.path = "/";
	root.has_client_max_body_size = false;

	LocationConfig images;
	images.path = "/images";
	images.has_client_max_body_size = true;
	images.client_max_body_size = 500;

	LocationConfig icons;
	icons.path = "/images/icons";
	icons.has_client_max_body_size = true;
	icons.client_max_body_size = 200;

	serverConfig.locations.push_back(root);
	serverConfig.locations.push_back(images);
	serverConfig.locations.push_back(icons);

	Client client(42, 0);

	// Pretend this request has already received the server-level default.
	client.setEffectiveMaxBodySize(
		serverConfig.client_max_body_size
	);

	// Set the request path for testing.
	client.setRequestPath("/images/icons/logo.png");

	client.matchLocation(serverConfig.locations);

	size_t locationIndex =
		client.getLocationIndex();

	assert(locationIndex != static_cast<size_t>(-1));

	const LocationConfig& location =
		serverConfig.locations[locationIndex];

	if (location.has_client_max_body_size)
	{
		client.setEffectiveMaxBodySize(
			location.client_max_body_size
		);
	}

	assert(locationIndex == 2);
	assert(client.getEffectiveMaxBodySize() == 200);

	std::cout << "PASS: longest location matched" << std::endl;
	std::cout << "location index: "
		<< locationIndex << std::endl;
	std::cout << "effective max body size: "
		<< client.getEffectiveMaxBodySize() << std::endl;

	return 0;
}