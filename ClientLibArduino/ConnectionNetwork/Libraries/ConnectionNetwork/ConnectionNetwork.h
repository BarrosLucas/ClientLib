/*
 * Apartamento.h
 *
 *  Created on: 20 de mar de 2019
 *      Author: lucas
 */
#ifndef CONNECTIONNETWORK_H_
#define CONNECTIONNETWORK_H_
 
 
#include <ESP8266WiFi.h>
#include <Thread.h>

class ConnectionNetwork{
public:
	ConnectionNetwork();

	
	
	void networkConnect(const char* ssid, const char* password);
	bool establishConnection(const char* host, const int port, void (*onConnect)());
	void onDataCallback(void (*onData)(String data));
	bool sendData(char * data);
	
	static String readingData();

	virtual ~ConnectionNetwork();

private:

	void (*onData)(String data);
	
	void readingDataReceived(void (*onData)(String data));
};

#endif 
