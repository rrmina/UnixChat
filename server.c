#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <crypt.h>

#define PORT "8989"   // port we're listening on

FILE *fd;

int connections = 0, for_logout = 0, for_logout_broadcast = 0;

// Handles Ctrl + C calls
void sig_usr(int signo)
{
	printf("\n");
	if(signo==SIGINT) {
		for_logout = 1;
		for_logout_broadcast = 1;
		if(connections==0) {
			printf("\033[31m\nServer closed. \n\033[0m");
			exit(0);
		}
		return;
	}
	else
		printf("Unknown signal number\n");
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*** Sends message to clients ****/
void send_message(char* message, int socket_max, fd_set master, int listener_socket, int* check_for_login, int sender_socket, int login_error)
{
	int temporary_socket_count;
	for(temporary_socket_count = 0; temporary_socket_count <= socket_max; temporary_socket_count++) {
		// send to everyone!
		if (FD_ISSET(temporary_socket_count, &master)) {
			// except the listener and ourselves
			if (temporary_socket_count != listener_socket) {
				if (sender_socket == -2 && login_error == -2) {
					if (send(temporary_socket_count, message, strlen(message)+1, 0) == -1) {
						perror("send");
					}
				}
				else if (login_error == -3 && temporary_socket_count != sender_socket && temporary_socket_count != listener_socket) {
					if (send(temporary_socket_count, message, strlen(message)+1, 0) == -1) {
						perror("send");
					}
				} else {
					if (temporary_socket_count != sender_socket && check_for_login[temporary_socket_count] == login_error) {
						if (send(temporary_socket_count, message, strlen(message)+1, 0) == -1) {
							perror("send");
						}
					}
				}
			}
		}
	}
	return;
}

int main(void)
{
	fd_set master; // master file descriptor list
	fd_set read_fds; // temp file descriptor list for select()
	int fdmax;    // maximum file descriptor number

	int listener; // listening socket descriptor
	int newfd;    // newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr; // client address
	socklen_t addrlen;

	char buf[256]; // buffer for client data
	int nbytes;
	int c;
	int check_for_login[100]; //assuming that the limit of client is 96, otherwise, use malloc and realloc
	int account_num[100];
	//initialize check_for_login
	int count;
	for (count = 0; count<100; count++) {
		check_for_login[count] = 0;
		account_num[count] = -1;
	}
	char user[100][20];
	char user2[100][20];
	char pass[100][300];
	char whosonline[300];
	char logout[140];
	char login[140];
	char remoteIP[INET6_ADDRSTRLEN];

	int yes=1;    // for setsockopt() SO_REUSEADDR, below
	int i, j, rv; // socket counts
	char *hashed = malloc(100);
	char salt[30] = "$1$vf3r32bs64612$"; // random salt
	struct addrinfo hints, *ai, *p;

	struct sigaction act;
	act.sa_handler = sig_usr;
	act.sa_flags = SA_RESTART;
	sigemptyset(&act.sa_mask);

	//Ctrl + C Handler
	/*if(*/ sigaction(SIGINT, &act, NULL);/* == SIG_ERR)
	                                         //	printf("Error creating SIG_INT\n");*/

	//printf("irc.server.com topic set to -----\n");

	FD_ZERO(&master); // clear the master and temp sets
	FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}


	for(p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) {
			continue;
		}

		// lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL) {
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

	// listen
	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(3);
	}

	// add the listener to the master set
	FD_SET(listener, &master);

	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

	// main loop
	for(;; ) {
		if (for_logout_broadcast) {
			printf("\033[31mServer is shutting down. Waiting for client/s to disconnect.\n\033[0m");

			// Send warning to all connected clients
			char mesg[140];
			sprintf(mesg, "\033[31m<ADMIN> SERVER IS SHUTTING DOWN, PLEASE LOG OUT NOW TO AVOID LOSS OF DATA.\n\033[0m");
			send_message(mesg, fdmax, master, listener, check_for_login, -1, -1);
		}
		for_logout_broadcast = 0;
		
		read_fds = master; // copy it
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			//perror("select");
			//exit(4);
			continue;//WOW, had debugged for 3 hours just for this one line and a couple of "//"
		}

		// run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				if (i == listener) {
					// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(listener,
					               (struct sockaddr *)&remoteaddr,
					               &addrlen);

					if (newfd == -1) {
						perror("accept");
					} else {
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax) { // keep track of the max
							fdmax = newfd;
						}
						connections++;
						printf("selectserver: new connection from %s on "
						       "socket %d\n",
						       inet_ntop(remoteaddr.ss_family,
						                 get_in_addr((struct sockaddr*)&remoteaddr),
						                 remoteIP, INET6_ADDRSTRLEN),
						       newfd);
						//printf("newfd: %i\n", newfd);

						check_for_login[newfd] = 1;
					}
				} else {
					// handle data from a client
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						// got error or connection closed by client
						if (nbytes == 0) {
							// connection closed
							printf("<%s> has been disconnected. socket %d\n", user[i], i);
							connections--;
							if (for_logout) {
								if (connections==0) {
									printf("\033[31mServer closed.\n\033[0m");
									exit(0);
								}
							}
							// Send message to every client
							sprintf(logout, "<%s> has logged out.\n", user[i]);
							send_message(logout, fdmax, master, listener, check_for_login, i, 0);
							strcpy(user[i], "");
						} else {
							perror("recv");
						}
						close(i); // bye!
						FD_CLR(i, &master); // remove from master set
					} else {
						char username[20];
						char password[300];
						int ref_num = 0;
						if (access( "accounts.txt", F_OK) == -1) {
							fd = fopen("accounts.txt", "w");
							fclose(fd);
						}
						fd = fopen("accounts.txt", "r+");
						buf[nbytes-1] = '\0';
						if (check_for_login[i] == 1) {
							if(strcmp(buf, "1")==0) {
								send(i, "Login\nUsername: ", strlen("Login\nUsername: ")+1, 0);
								check_for_login[i] = 2;
							}
							else if(strcmp(buf, "2")==0) {
								send(i, "Create Account\nUsername: ", strlen("Create Account\nUsername: ")+1, 0);
								check_for_login[i] = 4;

							}
							else {
								send(i, "Invalid Choice\n1 - Login\n2 - Create Account\n", strlen("Invalid Choice\n1 - Login\n2 - Create Account\n") + 1, 0);
								check_for_login[i] = 1;
							}
						}
						else if (check_for_login[i] == 2) {
							if (strcmp(buf, "")==0) {
								send(i, "Invalid username. (blank)\nUsername: ", strlen("Invalid username. (blank)\nUsername: ")+1, 0);
								check_for_login[i] = 2;
							}
							else {
								while(!feof(fd)) {
									ref_num++;
									fscanf(fd, "%s %s\n", username, password);
									if (strcasecmp(buf, username)==0) {
										account_num[i] = ref_num;
										strcpy(user2[i], username);
										break;
									}
									else
										account_num[i] = -1;
								}

								check_for_login[i] = 3;
								send(i, "\nPassword: ", strlen("\nPassword: ")+1, 0);
							}
						}
						else if (check_for_login[i] == 3) {
							if (strcmp(buf, "")==0) {
								send(i, "Invalid password. (blank)\nPassword: ", strlen("Invalid password. (blank)\nPassword: ")+1, 0);
								check_for_login[i] = 3;
							}
							else {
								while(!feof(fd)) {
									ref_num++;
									fscanf(fd, "%s %s\n", username, password);
									char *hashed = malloc(100);
									hashed = crypt(buf, salt);
									if (strcmp(hashed, password)==0) {
										if(ref_num == account_num[i]) {
											check_for_login[i] = 0;
											send(i, "Admin has given you voice.\n", strlen("Admin has given you voice.\n")+1, 0);
											printf("<%s> has connected.\n", user2[i]);
											strcpy(user[i], user2[i]);

											// Send to every client except server and the one that just logged in
											sprintf(login, "<%s> has logged in.\n", user[i]);
											send_message(login, fdmax, master, listener, check_for_login, i, 0);
											break;
										}
										//send(newfd, "<User> has logged in\n", strlen("<User> has logged in\n"), 0);
									}
								}
								if (check_for_login[i] != 0) {
									send(i, "Wrong Password or Invalid Account\nUsername: ", strlen("Wrong Password or Invalid Account\nUsername: ")+1, 0);
									check_for_login[i] = 2;
								}
							}
						}
						else if (check_for_login[i] == 4) {
							while(!feof(fd)) {
								fscanf(fd, "%s %s\n", user[i], pass[i]);
								if (strcmp(buf, "")==0) {
									send(i, "Invalid username. (blank)\nUsername: ", strlen("Invalid username. (blank)\nUsername: ")+1, 0);
									check_for_login[i] = 4;
									break;
								}
								else if (strcasecmp(buf, user[i])==0) {
									send(i, "Username is already used. Please choose a new username.\nUsername: ", strlen("Username is already used. Please choose a new username.\nUsername: ")+2, 0);
									check_for_login[i] = 4;
									break;
								}
								else {
									strcpy(user[i], buf);
									check_for_login[i] = 5;
								}
							}
							if (check_for_login[i] == 5)
								send(i, "\nPassword: ", strlen("\nPassword: ")+1, 0);
						}
						else if(check_for_login[i] == 5) {
							strcpy(pass[i], buf);
							send(i, "\nConfirm Password: ", strlen("\nConfirm Password: ")+1, 0);
							check_for_login[i] = 6;
						}
						else if (check_for_login[i] == 6) {
							if (strcmp(buf, pass[i])==0) {
								char *hashed = malloc(100);
								hashed = crypt(buf, salt);
								check_for_login[i] = 2;
								fseek(fd, 0, SEEK_END);
								long file_end = ftell(fd);
								fseek(fd, file_end, SEEK_SET);
								fprintf(fd, "%s %s\n", user[i], hashed);
								fflush(fd); //damn this, fixing fprintf not writing to the file while returning positive. Internet sure is helpful.
								send(i, "Successfully created account!\nLogin\nUsername: ", strlen("Successfully created account!\nLogin\nUsername: ")+2, 0);
							}
							else {
								send(i, "Passwords do not match.\nCreate Account\nUsername: ", strlen("Passwords do not match.\nCreate Account\nUsername: ") + 1, 0);
								check_for_login[i] = 4;
							}

						}
						else {
							// we got some data from a client
							int online_number = 0;
							strcpy(whosonline, "");
							char command[200];
							char person[200];
							char message[200];
							int wala=0;
							char message2[200];
							sscanf(buf, "%s %s %s", command, person, message);
							printf("%s %s %s -\n", command, person, message);
							if (strcmp("/PM", command)==0) {
								for(j = 0; j <= fdmax; j++) {
									if (FD_ISSET(j, &master)) {
										// Send to the target person
										if (strcmp(user[j], person) == 0) {
											sprintf(message2, "[PM]<%s> %s\n", user[i], message);
											send(j,message2, strlen(message2)+1, 0);
											wala = 1;
											break;
										}
									}
								}
								if (wala==0) {
									sprintf(message, "Admin: %s isn't connected.\n", person);
									send(i, message, strlen(message)+1, 0);
								}
							}
							else if (strcmp("/whosonline", buf)==0) {
								for(j = 0; j <= fdmax; j++) {
									if (FD_ISSET(j, &master)) {
										// Send list to the sender
										if (j != listener && j != i && check_for_login[j] == 0) {
											printf("%s\n", user[j]);
											strcat(whosonline, user[j]);
											strcat(whosonline, "\n");
											online_number++;
										}
									}
								}
								//printf("%s\n", whosonline);
								if (online_number)
									send(i, whosonline, strlen(whosonline) + 1, 0);
								else
									send(i, "You're the only one logged in.\n", strlen("You're the only one logged in.\n") + 1, 0);
							}
							else {
								/*for(j = 0; j <= fdmax; j++) {
									// send to everyone!
									if (FD_ISSET(j, &master)) {
										// except the listener and ourselves
										if (j != listener && j != i && check_for_login[j] == 0) {
											char mesg[140];
											sprintf(mesg, "<%s> %s\n", user[i], buf);
											if (send(j, mesg, strlen(mesg)+1, 0) == -1) {
												perror("send");
											}
										}
									}
								}*/
								char mesg[140];
								sprintf(mesg, "<%s> %s\n", user[i], buf);
								send_message(mesg, fdmax, master, listener, check_for_login, i, -3);
							}
							fclose(fd);
						}
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!

	return 0;
}
