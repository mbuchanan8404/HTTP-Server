/* Matthew Buchanan */
/* This program is based on existing code by Dr. Hiro Fujinoki and may not be reproduced */

/* CS 447 Network Communications (Fall 2019)   */
/* the sample server process (for Project #1)  */

// Updated for Visual Studio 2019
// include link-library: wsock32.lib or ws2_32.lib (from Project Properties)

// included wsock32.lib for this project in project properties->linker->commandLine->

// Last Updated: 5:11 PM, September 5, 2019
// Last Updated by MB: 10/3/2019

#define _CRT_SECURE_NO_WARNINGS  // avoid C4996 warnings (needed for Visual Studio 2019)
//#define _CRT_NONSTDC_NO_WARNINGS // I added this one, suppress even more C4996 warnings
/*----- Include files -------------------------------------------------------*/
#include <iostream>
#include <string>           /* Needed for memcpy() and strcpy()              */
#include <time.h>           /* Needed for clock() and CLK_TCK                */
#include <fcntl.h>          /* for O_WRONLY, O_CREAT                         */
#include <windows.h>        /* Needed for all Winsock stuff                  */
#include <sys\timeb.h>      /* Needed for ftime() and timeb structure        */
#include <io.h>				/* Makes _open work								 */	
#include <fstream>			/* For reading files to be transfered			 */		
#include <thread>			/* Library for making server multithreaded       */
#include <mutex>			/* A very basic lock for file access			 */

using namespace std;

/*----- Defines -------------------------------------------------------------*/
#define  SERV_PORT_NUM     8050   /* Server Side Port number                 */
#define  MAX_CONNECTIONS      3   /* up to 3 connections			         */

//-----HTTP response messages ----------------------------------------------#
//#define OK_IMAGE    "HTTP/1.0 200 OK\nContent-Type:image/gif\n"
//#define OK_TEXT     "HTTP/1.0 200 OK\nContent-Type:text/html\n\n"
#define NOTOK_404   "HTTP/1.0 404 Not Found\nContent-Type:text/html\n\n"
#define MESS_404    "<html><body><h1>FILE NOT FOUND</h1></body></html>"


/* Basic file lock for the httpHandler() threads to synchronise on */
mutex fileLock;

/* This function acts as a request handler thread */
void httpHandler(int csocket);


// MODULE MAIN ===============================================================
void main(int)
{
	int status;                   // status-code from socket APIs

	int server_socket;            // socket descriptor for primary connection
	int child_socket;             // socket descriptor for duplicated connection

	int size;                     // size of a sock_addr structure

	/* load the Winsock library ---------------------------------------------- */
	WORD wVersionRequested = MAKEWORD(1, 1); /* Version 1.1                    */
	WSADATA wsaData;                         /* for the loaded winsock library */

	/* the sockaddr structures ----------------------------------------------- */
	struct sockaddr_in   server_addr;     /* for server (IP address and port #)*/
	struct sockaddr_in   client_addr;     /* for client (IP address and port #)*/


	/* activate the winsock libabry ------------------------------------------ */
	WSAStartup(wVersionRequested, &wsaData);

	/* create a new TCP/IP socket -------------------------------------------- */
	/* AF_INET = "IP" and SOCK_STREAM = "TCP"                                  */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);

	/* error check for "socket API" ------------------------------------------ */
	if (server_socket == INVALID_SOCKET)
	{
		cout << ("Error at socket(): %ld\n", WSAGetLastError());
	}

	/* set up "sockaddr structure" for this server --------------------------- */
	server_addr.sin_family = AF_INET;                         /* "IP" protocol */
	server_addr.sin_port = htons(int(SERV_PORT_NUM));         /* TCP port #    */
	server_addr.sin_addr.s_addr = htons(INADDR_ANY); /* Server-side IP address */

	/* connect this server process to a designated TCP port ------------------ */
	status = bind(server_socket, (struct sockaddr*) & server_addr, sizeof server_addr);

	/* error check for bind API ---------------------------------------------- */
	if (status < 0)
	{
		cout << "BIND Error " << WSAGetLastError() << " " << WSAGetLastError() << endl;
	}

	/* start listening to the TCP port --------------------------------------- */
	status = listen(server_socket, MAX_CONNECTIONS);

	/* error check for listen API -------------------------------------------- */
	if (status < 0)
	{
		cout << "LISTEN Error " << WSAGetLastError() << endl;
	}

	/* the server-side main (infinite) loop ---------------------------------- */
	while (true)
	{
		/* wait for a new client on the primary TCP port ----------- */
		size = sizeof(client_addr);
		child_socket = accept(server_socket, (struct sockaddr*) & client_addr, &size);

		/* if a TCP connection is successfully established with this client -- */
		if (child_socket != INVALID_SOCKET)
		{
			cout << "Connection from " << inet_ntoa(client_addr.sin_addr)
				<< " arrived (Client-Side TCP Port No = " << htons(client_addr.sin_port) << ")\n";
			thread t(httpHandler, child_socket);
			t.detach();
			cout << "Child thread started\n";
		}
	} // END OF THE WHILE-LOOP ////////////////////
}
// END OF THE SERVER PROCESS //////////////////////////////////////////////



/* ===== This function is a thread for each http request ====================== */
void httpHandler(int csocket)
{
	/* Local variables */
	int clientSocket = csocket;
	int bufsize = 500;
	char in_buf[500];					/* The in-coming buffer */
	char out_buf[500];					/* The out-going buffer */
	char* file_name;					// File name
	int fileSize = 0;


	/* Clean up the buffers --------------------------------- */
	for (int j = 0; j < bufsize; j++)
	{
		out_buf[j] = '\0';
		in_buf[j] = '\0';
	}

	/* Receive a client request ----------------------------- */
	recv(clientSocket, in_buf, bufsize, 0);
	strtok(in_buf, " ");	// the first call to strtok skips “GET”
	file_name = strtok(NULL, " ");	// the second call to strtok grabs the file name
	if (file_name == NULL)
	{
		closesocket(clientSocket);
		return;
	}

	/* Check the server for the file ------------------------ */
	fileLock.lock();	// Lock critical region of thread
	string fpath = ".";
	fpath += file_name;
	ifstream file(fpath, ios::binary);

	/* If the file isn't found, return 404*/
	if (!file)
	{
		file.close();
		fileLock.unlock();	// Unlock critical region in this branch
		strcpy(out_buf, NOTOK_404);
		send(clientSocket, out_buf, 48, 0);
		for (int j = 0; j < bufsize; j++)
		{
			out_buf[j] = '\0';
		}
		strcpy(out_buf, MESS_404);
		send(clientSocket, out_buf, 50, 0);
		closesocket(clientSocket);
		return;
	}

	/* Read the whole file into tempbuf at once ---------------- */
	file.seekg(0, ios::end);
	fileSize = file.tellg();
	file.seekg(0, ios::beg);
	char* tempbuf;
	tempbuf = new char[fileSize];
	for (int i = 0; i < fileSize; i++)
	{
		tempbuf[i] = '\0';
	}
	file.read(tempbuf, fileSize);
	file.close();
	fileLock.unlock();	// Unlock and exit critical region

	/* Detect which file type was requested and send appropriate http response */
	string s = "";
	s += "HTTP/1.0 200 OK\n";
	s += "Connection: close\n";
	s += "Content-Length: " + to_string(fileSize) + '\n';
	if (strstr(file_name, ".gif") != NULL)
	{
		s += "Content-Type:image/gif\n\n";
	}
	else if (strstr(file_name, ".jpg") != NULL)
	{
		s += "Content-Type: image/jpg\n\n";
	}
	else
	{
		s += "Content-Type: text/html; charset=UTF-8\n\n";
	}
	strcpy(out_buf, s.c_str());
	send(clientSocket, out_buf, s.length(), 0);

	/* Clean up the buffer ------------------------------------- */
	for (int j = 0; j < bufsize; j++)
	{
		out_buf[j] = '\0';
	}

	/* Send the contents of tempbuf to client one chunk at a time */
	char* tarr = new char[bufsize];
	for (int i = 0; i < bufsize; i++)
	{
		tarr[i] = '\0';
	}
	int segcounter = 0; // a counter for which chunk we're on
	for (int i = 0; i < fileSize; i++)
	{
		/* If the loop has filled the out buffer time to send a full chunk */
		if (i != 0 && ((i % bufsize) == 0))
		{
			segcounter++;
			memcpy(out_buf, tarr, bufsize);
			send(clientSocket, out_buf, bufsize, 0);
			/* If we only have enough data left to send a partial chunk, break */
			if ((fileSize - ((segcounter)*bufsize)) < bufsize)
			{
				break;
			}
			for (int j = 0; j < bufsize; j++)
			{
				tarr[j] = '\0';
				out_buf[j] = '\0';
			}
		}
		tarr[i - (segcounter * bufsize)] = tempbuf[i]; // do the actual byte copy
	}

	/* Now send the last partial chunk if there is one ---------- */
	if (segcounter * bufsize < fileSize) {
		for (int j = 0; j < bufsize; j++)
		{
			tarr[j] = '\0';
			out_buf[j] = '\0';
		}
		int g = 0;
		for (int i = (segcounter * bufsize); i < fileSize; i++)
		{
			tarr[g] = tempbuf[i];
			g++;
		}
		memcpy(out_buf, tarr, g);
		send(clientSocket, out_buf, g, 0);
	}

	/* Clean up memory, close the socket, and end the thread ----- */
	delete[] tempbuf;
	delete[] tarr;
	closesocket(clientSocket);
}