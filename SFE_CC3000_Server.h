/**
 * @file    SFE_CC3000_Server.h
 * @brief   Library for the SparkFun CC3000 shield and breakout boards
 * @author  William Bernardet (https://github.com/williambernardet)
 * 
 * @copyright   This code is public domain but you buy me a beer if you use
 * this and we meet someday (Beerware license).
 * 
 * The server library provides functions to create a service.
 */
 
#ifndef SFE_CC3000_SERVER_H
#define SFE_CC3000_SERVER_H

#include <Arduino.h>

#include "SFE_CC3000.h"
#include "SFE_CC3000_Client.h"
#include "utility/socket.h"                 // Needed for socket communications
#include "Server.h"
#include "Client.h"

/* Constants for IP protocol types */
#define TCP     IPPROTO_TCP
#define UDP     IPPROTO_UDP
#define SERVER_MAX_CLIENTS 3 


class SFE_CC3000_ClientRef : public Client {
public:
	SFE_CC3000_ClientRef(SFE_CC3000_Client* client);
	SFE_CC3000_ClientRef(const SFE_CC3000_ClientRef& client);
	
	// Return true if the referenced client is connected.  This is provided for
	// compatibility with Ethernet library code.
	operator bool();
	
	// Below are all the public methods of the client class:
  	int connect(IPAddress ip, uint16_t port);
  	int connect(const char *host, uint16_t port);

  	uint8_t connected(void);
  	size_t write(uint8_t c);

  	size_t fastrprint(const char *str);
	size_t fastrprintln(const char *str);
	size_t fastrprint(char *str);
  	size_t fastrprintln(char *str);
  	size_t fastrprint(const __FlashStringHelper *ifsh);
  	size_t fastrprintln(const __FlashStringHelper *ifsh);

  	size_t write(const void *buf, uint16_t len, uint32_t flags = 0);
  	int read(void *buf, uint16_t len, uint32_t flags = 0);
  	int read(void);
  	int32_t close(void);
  	int available(void);

  	int read(uint8_t *buf, size_t size);
  	size_t write(const uint8_t *buf, size_t size);
  	int peek();
  	void flush();
  	void stop();

private:
	// Hide the fact that users are really dealing with a pointer to a client
  	// instance.  Note: this class does not own the contents of the client
  	// pointer and should NEVER attempt to free/delete this pointer.
	SFE_CC3000_Client *client_;
};

/* CC3000 Client class */
class SFE_CC3000_Server : public Server {
public:
    SFE_CC3000_Server(SFE_CC3000 &cc3000, uint16_t port);
    ~SFE_CC3000_Server();
  // Return the index of a client instance with data available to read.
  // This is useful if you need to keep track of your own client state, you can
  // index into an array of client state based on the available index returned
  // from this function.  Optional boolean parameter returns by reference true
  // if the available client is connecting for the first time.
  int8_t availableIndex(bool *newClient = NULL);
  // Get a client instance from a given index.
  SFE_CC3000_ClientRef getClientRef(int8_t clientIndex);
  // Return a reference to a client instance which has data available to read.
  SFE_CC3000_ClientRef available();
  // Initialize the server and start listening for connections.
  virtual void begin();
  // Write data to all connected clients.  Buffer is a pointer to an array
  // of bytes, and size specifies how many bytes to write from the buffer.
  // Return the sum of bytes written to all clients.
  virtual size_t write(const uint8_t *buffer, size_t size);
  // Write a byte value to all connected clients.
  // Return the sum of bytes written to all clients.
  virtual size_t write(uint8_t value);

private:
	bool acceptNewConnections();
private:
    SFE_CC3000 *cc3000_;
  	// Store the clients in a simple array.
  	SFE_CC3000_Client clients_[SERVER_MAX_CLIENTS];
	// The port this server will listen for connections on.
 	uint16_t port_;
  	// The id of the listening socket.
  	uint32_t listenSocket_;
};

#endif