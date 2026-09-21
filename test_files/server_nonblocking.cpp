#include "Server.hpp"
#include <fcntl.h>      // fcntl(), F_GETFL, O_NONBLOCK
#include <unistd.h>     // close(), alarm()
#include <cerrno>       // errno, EAGAIN, EWOULDBLOCK
#include <iostream>     // std::cout

int main()
{
	/*
     * Purpose of this test is to check whether the Server correctly uses non-blocking sockets.
     *
     * It verifies five things:
     *
     * 1. setNonBlocking() successfully adds O_NONBLOCK.
     * 2. Existing file status flags are preserved.
     * 3. recv() on an empty non-blocking socket returns immediately with EAGAIN or EWOULDBLOCK instead of blocking.
     * 4. createListeningSocket() creates a non-blocking listening socket.
     * 5. accept() on a listening socket with no pending client connection
     *    returns immediately with EAGAIN or EWOULDBLOCK.
     *
     * The test returns 0 if all checks pass, and 1 if any check fails.
     */

    // Test 1 & 2:
    // Create a vector containing one ServerConfig,
	// this is a minimum configuration to construct a Server object.

    std::vector<ServerConfig> configs(1);
    configs[0].port = 0; //Port 0 tells the operating system to automatically choose an available port when bind() is called.
    Server server(configs);

    int pair[2]; // to reserve space for 2 fds
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1) // to create 2 connected stream sockets
        return 2;

	// read and record the current file status flags of pair[0]
    int before = fcntl(pair[0], F_GETFL, 0);

	// call our own setNonBlocking() to add O_NONBLOCK to pair[0], will return true if success
    bool changed = server.setNonBlocking(pair[0]);

	// read and record changed flags
    int after = fcntl(pair[0], F_GETFL, 0);

    /*
     * Verify that setNonBlocking() worked correctly.
     *
     * This checks:
     *
     * 1. changed is true.
     * 2. Reading the original flags succeeded.
     * 3. Reading the new flags succeeded.
     * 4. O_NONBLOCK is present after the change.
     * 5. All flags that existed before are still present.
     *
     * The last condition checks that we added O_NONBLOCK
     * instead of replacing all existing flags.
     */
    bool flags = changed && before != -1 && after != -1
        && (after & O_NONBLOCK) && (after & before) == before;

	// test 3
	// create a one-byte buffer for recv()
    char byte;
	// set a 3-second alarm as a saftry mechanism
    alarm(3);

	/*
     * Try to read one byte from pair[0].
     *
     * pair[1] has not sent any data.
     *
     * If pair[0] is blocking: recv() waits for data.
     * If pair[0] is non-blocking: recv() should immediately return -1, with errno set to EAGAIN or EWOULDBLOCK.
     */
    ssize_t count = recv(pair[0], &byte, 1, 0);
    bool wouldBlock = count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);

	// disable the alarm
    alarm(0);
	// Test error handling with an invalid file descriptor.
    bool invalid = !server.setNonBlocking(-1);
    close(pair[0]);
    close(pair[1]);

	// test 4 & 5
    int listener = server.createListeningSocket(configs[0]);
    int listenerFlags = fcntl(listener, F_GETFL, 0);
    bool listening = listenerFlags != -1 && (listenerFlags & O_NONBLOCK);
    alarm(3);
    int accepted = accept(listener, NULL, NULL);
    bool acceptWouldBlock = accepted == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);
    alarm(0);
    if (accepted >= 0)
        close(accepted);
    close(listener);

	// print all results
    std::cout << "preserve flags + O_NONBLOCK: " << flags
              << "\nempty recv returns EAGAIN: " << wouldBlock
              << "\ninvalid fd rejected: " << invalid
              << "\nlistener O_NONBLOCK: " << listening
              << "\nempty accept returns EAGAIN: " << acceptWouldBlock << '\n';

    return flags && wouldBlock && invalid && listening && acceptWouldBlock ? 0 : 1;
}
