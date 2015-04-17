/**
 * @file    SFE_CC3000_Server.cpp
 * @brief   Library for the SparkFun CC3000 shield and breakout boards
 * @author  William Bernardet (https://github.com/williambernardet)
 * 
 * @copyright   This code is public domain but you buy me a beer if you use
 * this and we meet someday (Beerware license).
 * 
 */
 
#include <Arduino.h>
#include <SPI.h>
 
#include "common.h"
#include "SFE_CC3000.h"
#include "SFE_CC3000_Server.h"
#include "utility/socket.h"

#define HANDLE_NULL(v, r) if (v == NULL) { return r; }
#ifdef DEBUG
#define DEBUG_PRINTLN_F(text) { Serial.println(F(text)); }
#else
#define DEBUG_PRINTLN_F(text) 
#endif

SFE_CC3000_ClientRef::SFE_CC3000_ClientRef(SFE_CC3000_Client* client)
{
	client_ = client;
}

SFE_CC3000_ClientRef::SFE_CC3000_ClientRef(const SFE_CC3000_ClientRef& client)
{
	client_ = client.client_;
}

// Return true if the referenced client is connected.  This is provided for
// compatibility with Ethernet library code.
SFE_CC3000_ClientRef::operator bool() {
  return client_ != NULL && *client_;
}

// Below are wrappers around the public client functions.  These hide the fact that users
// are dealing with a reference to a client instance and allow code to be written using
// value semantics like in the Ethernet library.
int SFE_CC3000_ClientRef::connect(IPAddress ip, uint16_t port) {
  HANDLE_NULL(client_, false);
  return client_->connect(ip, port);
}

int SFE_CC3000_ClientRef::connect(const char *host, uint16_t port) {
  HANDLE_NULL(client_, false);
  return client_->connect(host, port);
}

uint8_t SFE_CC3000_ClientRef::connected(void) {
  HANDLE_NULL(client_, false);
  return client_->connected();
}

size_t SFE_CC3000_ClientRef::write(uint8_t c) {
  HANDLE_NULL(client_, 0);
  return client_->write(c);
}

int SFE_CC3000_ClientRef::read(void) {
  HANDLE_NULL(client_, 0);
  return client_->read();
}

int32_t SFE_CC3000_ClientRef::close(void) {
  HANDLE_NULL(client_, 0);
  return client_->close();
}

int SFE_CC3000_ClientRef::available(void) {
  HANDLE_NULL(client_, 0);
  return client_->available();
}

int SFE_CC3000_ClientRef::read(uint8_t *buf, size_t size) {
  HANDLE_NULL(client_, 0);
  return client_->read(buf, size);
}

size_t SFE_CC3000_ClientRef::write(const uint8_t *buf, size_t size) {
  HANDLE_NULL(client_, 0);
  return client_->write(buf, size);
}

int SFE_CC3000_ClientRef::peek() {
  HANDLE_NULL(client_, 0);
  return client_->peek();
}

void SFE_CC3000_ClientRef::flush() {
  if (client_ != NULL) client_->flush();
}

void SFE_CC3000_ClientRef::stop() {
  if (client_ != NULL) client_->stop();
}


/**************************************************************************/
/*
  SFE_CC3000_Server implementation
*/
/**************************************************************************/
SFE_CC3000_Server::SFE_CC3000_Server(SFE_CC3000 &cc3000, uint16_t port)
{
	port_ = port;
	listenSocket_ = -1;
	cc3000_ = &cc3000;
}

SFE_CC3000_Server::~SFE_CC3000_Server() {
}

// Return index of a client with data available for reading. Can be turned
// into a client instance with getClientRef().  Accepts an optional parameter
// to return a boolean (by reference) indicating if available client is connecting
// for the first time.
int8_t SFE_CC3000_Server::availableIndex(bool *newClient) {
  bool newClientCreated = acceptNewConnections();

  if (newClient)
    *newClient = newClientCreated;

  // Find the first client which is ready to read and return it.
  for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
    if (clients_[i].connected() && clients_[i].available() > 0) {
      return i;
    }
  }

  return -1;
}


// Given the index of client, returns the instance of that client for reading/writing
SFE_CC3000_ClientRef SFE_CC3000_Server::getClientRef(int8_t clientIndex) {
  if (clientIndex != -1) {
    return SFE_CC3000_ClientRef(&clients_[clientIndex]);
  }
  
  // Couldn't find a client ready to read, so return a client that is not 
  // connected to signal no clients are available for reading (convention
  // used by the Ethernet library).
  return SFE_CC3000_ClientRef(NULL);
}


// Return a reference to a client instance which has data available to read.
SFE_CC3000_ClientRef SFE_CC3000_Server::available() {
  return getClientRef(availableIndex(NULL));
}


// Initialize the server and start listening for connections.
void SFE_CC3000_Server::begin() {
  DEBUG_PRINTLN_F("SFE_CC3000_Server::begin()");
  // Set the CC3000 inactivity timeout to 0 (never timeout).  This will ensure 
  // the CC3000 does not close the listening socket when it's idle for more than 
  // 60 seconds (the default timeout).  See more information from:
  // http://e2e.ti.com/support/low_power_rf/f/851/t/292664.aspx
  unsigned long aucDHCP       = 14400;
  unsigned long aucARP        = 3600;
  unsigned long aucKeepalive  = 30;
  unsigned long aucInactivity = 0;
  //cc3k_int_poll();
  if (netapp_timeout_values(&aucDHCP, &aucARP, &aucKeepalive, &aucInactivity) != 0) {
    DEBUG_PRINTLN_F("Error setting inactivity timeout!");
    return;
  }
  // Create a TCP socket
  //cc3k_int_poll();
  DEBUG_PRINTLN_F("SFE_CC3000_Server::begin() - socket");
  int16_t soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (soc < 0) {
    DEBUG_PRINTLN_F("Couldn't create listening socket!");
    return;
  }
  // Set the socket's accept call as non-blocking.
  //cc3k_int_poll();
  char arg = SOCK_ON; // nsd: looked in TI example code and they pass this as a 'short' in one example, and 'char' in two others. 'char' seems as likely work, and has no endianess issue
  if (setsockopt(soc, SOL_SOCKET, SOCKOPT_ACCEPT_NONBLOCK, &arg, sizeof(arg)) < 0) {
    DEBUG_PRINTLN_F("Couldn't set socket as non-blocking!");
    return;
  }
  // Bind the socket to a TCP address.
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(0);     // Listen on any network interface, equivalent to INADDR_ANY in sockets programming.
  address.sin_port = htons(port_);        // Listen on the specified port.
  //cc3k_int_poll();
  if (bind(soc, (sockaddr*) &address, sizeof(address)) < 0) {
    DEBUG_PRINTLN_F("Error binding listen socket to address!");
    return;
  }
  // Start listening for connections.
  // The backlog parameter is 0 as it is not supported on TI's CC3000 firmware.
  //cc3k_int_poll();
  if (listen(soc, 0) < 0) {
    DEBUG_PRINTLN_F("Error opening socket for listening!");
    return;
  }
  DEBUG_PRINTLN_F("SFE_CC3000_Server::begin() - end");
  listenSocket_ = soc;
}

// Write data to all connected clients.  Buffer is a pointer to an array
// of bytes, and size specifies how many bytes to write from the buffer.
// Return the sum of bytes written to all clients.
size_t SFE_CC3000_Server::write(const uint8_t *buffer, size_t size) {
  size_t written = 0;
  for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
    if (clients_[i].connected()) {
      written += clients_[i].write(buffer, size);
    }
  }
  return written;
}

// Write a byte value to all connected clients.
// Return the sum of bytes written to all clients.
size_t SFE_CC3000_Server::write(uint8_t value) {
  return write(&value, 1);
}

// Accept new connections and update the connected clients.
bool SFE_CC3000_Server::acceptNewConnections() {
  bool newClientCreated = false;
  // For any unconnected client, see if new connections are pending and accept
  // them as a new client.
  for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
    if (!clients_[i].connected()) {
      // Note: Because the non-blocking option was set for the listening
      // socket this call will not block and instead return SOC_IN_PROGRESS (-2) 
      // if there are no pending client connections. Also, the address of the 
      // connected client is not needed, so those parameters are set to NULL.
      //cc3k_int_poll();
      int soc = accept(listenSocket_, NULL, NULL);
      if (soc > -1) {
  		DEBUG_PRINTLN_F("SFE_CC3000_Server::acceptNewConnections() - accepted new connection");
        clients_[i] = SFE_CC3000_Client(*cc3000_, soc);
        newClientCreated = true;
      }
      // else either there were no sockets to accept or an error occured.
    }
  }
  return newClientCreated;
}

