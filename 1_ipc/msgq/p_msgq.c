/**
 * @author Nilesh PS
 * @description
 * This program demonstrates the use of message queues for Inter-Process Communication.
 * Server program shown below will accept bounded-length messages from clients, 
 * assign a unique id to every message, log the message along with the id and return a response
 * to the client
 *
 * NOTE : This program use POSIX message queue. It's available only in linux kernel version >= 2.6
 *        Refer https://users.cs.cf.ac.uk/Dave.Marshall/C/node25.html for message queues using
 *        sys/msg.h.
 */

// Server 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <error.h>
#include <stdbool.h>

int main() {
	// Queue descriptor for our own queue and for client's queue
	mqd_t my_qd, client_qd;
	// Configure queue attributes
	struct mq_attr attr;
	attr.mq_flags   = 0;
	attr.mq_maxmsg  = MSG_MAX_COUNT;
	attr.mq_msgsize = sizeof(svreq);
	attr.mq_curmsgs = 0;
	// Create a new queue with name SERVER_QNAME
	mq_unlink(SERVER_QNAME);
	if ((my_qd = mq_open(SERVER_QNAME, O_CREAT | O_RDONLY, 0755, &attr)) == -1) {
		perror("mq_open");
		exit(EXIT_FAILURE);
	}
	// Request and Response objects
	svreq req;
	svres res;
	// Each message received has a unique hit_no.
	int hit_no = 1;
	while (true) {
		ssize_t len;
		// Receive a message, blocking call.
		while ((len = mq_receive(my_qd, (char *)&req, sizeof(req) + 1, 0)) == -1 || len != sizeof(svreq)) {
			perror("mq_receive");
		}
		// Open the client queue as write only
		req.qname[QNAME_MAX_SIZE] = req.buffer[MSG_MAX_SIZE] = '\0';
		if( (client_qd = mq_open(req.qname, O_WRONLY)) == -1)
			perror("mq_open:client");
		// Print the message received from client
		printf("%d. Received \' %s \' from %ld\n\n", hit_no, req.buffer, (long)req.pid);
		// Prepare the reply
		sprintf(res.buffer, "Your message id = %d", hit_no++);
		// Send the reply object
		if ( mq_send(client_qd, (char *)&res, sizeof(svres), 0) == -1)
			perror("mq_write");
		// Close the connection with the queue
		mq_close(client_qd);
	} 
	return 0;
}

//client
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <error.h>
#include <stdbool.h>
#include <assert.h>
#include "__includes.h"

#define CLIENT_QNAME_PREFIX  "/sample-client-queue"

int main(int argc, char **argv) {
	/**
	 * Expects a string of length not greater than MSG_MAX_SIZE as
	 * a command line argument.
	 */
	assert( strlen(CLIENT_QNAME_PREFIX) < QNAME_MAX_SIZE - 5);
	int len;
	if (argc < 2) {
		fprintf(stderr, "No message!");
		exit(EXIT_FAILURE);
	}
	// Check the length of the argument
	len = strlen(argv[1]);
	if (len > MSG_MAX_SIZE) {
		fprintf(stderr, "Message size limit exceeded!");
		exit(EXIT_FAILURE);
	}
	// Queue descriptors for identifying server queue and our queue 
	// respectively.
	mqd_t qd_server, my_qd;
	// Connect with the server queue.
	qd_server = mq_open(SERVER_QNAME, O_WRONLY);
	// Failed to create a queue.
	if (qd_server == -1) {
		perror("mq_open:server");
		exit(EXIT_FAILURE);
	}
	// Create another message queue for receiving messages.
	struct mq_attr attr;
	attr.mq_flags   = 0;               // Set to O_NONBLOCK if asynchronous message passing is necessary.
	attr.mq_msgsize = sizeof(svres);   // Maximum size of message
	attr.mq_maxmsg  = MSG_MAX_COUNT;   // Maximum message that queue should hold at any instant.
	attr.mq_curmsgs = 0;               // Current no of mesages in queue.

	svreq req; // Request object
	svres res; // Response object
	// Since multiple instances(processes) of this program can exist at the same time, 
	// use process_id in the  queue name to ensure every process get a unique queue name. 
	sprintf(req.qname, "%s-%ld", CLIENT_QNAME_PREFIX, (long)getpid()); 
	req.pid = (long)getpid();
	// Create a queue for receiving messages
	if ((my_qd = mq_open(req.qname, O_CREAT | O_RDONLY, 0755, &attr)) == -1) {
		perror("mq_open:client");
		exit(EXIT_FAILURE);
	}
	strncpy(req.buffer, argv[1], MSG_MAX_SIZE);
	// Send the message
	if ( mq_send(qd_server, (char *)&req, sizeof(svreq), 0) == -1) {
		perror("mq_send");
		exit(EXIT_FAILURE);
	}
	// Wait for reply
	while ( (len = mq_receive(my_qd, (char *)&res, sizeof(res) + 1, 0)) == -1 || len != sizeof(res))
		perror("mq_receive");
	res.buffer[MSG_MAX_SIZE] = '\0';
	// Print the reply
	printf("Process id = %ld\n Reply :- %s\n\n", (long)getpid(), res.buffer);
	mq_close(qd_server);
	mq_close(my_qd);
	mq_unlink(req.qname);
	return 0;
}	
