#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <fcntl.h>

#define PORT 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

int server_fd = -1;
volatile sig_atomic_t exit_flag = 0;

void handle_signal(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting");
	// close socket here for accept() to return
	close(server_fd);
    exit_flag = 1;
}

int main(int argc, char *argv[]) {
	int daemon_mode = 0;
	for(int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0) {
			daemon_mode = 1;
		}
	}
	
    int client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return -1;
    }

    // Listen
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        return -1;
    }

	if (daemon_mode) {
		pid_t pid = fork();
		
		if (pid < 0) {
			perror("fork");
			exit(1);
		}
		
		if (pid > 0) {
			// parent exits
			exit(0);
		}
		
		// child continues as daemon
		syslog(LOG_INFO, "Daemon started");
	}
	
    while (!exit_flag) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
			if (exit_flag) {
				break;
			}
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        syslog(LOG_INFO, "Accepted connection from %s", ip);

        char *packet = NULL;
        size_t packet_size = 0;

        while (1) {
            ssize_t bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes <= 0) break;

            char *newbuf = realloc(packet, packet_size + bytes);
			if (!newbuf) {
				syslog(LOG_ERR, "realloc failed");
				free(packet);
				break;
			}
            packet = newbuf;
            memcpy(packet + packet_size, buffer, bytes);
            packet_size += bytes;

            // check for newline
            if (memchr(packet, '\n', packet_size)) {
                // append to file
                int fd = open(FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd >= 0) {
                    write(fd, packet, packet_size);
                    close(fd);
                }

                free(packet);
                packet = NULL;
                packet_size = 0;

                // send full file back
                fd = open(FILE_PATH, O_RDONLY);
                if (fd >= 0) {
                    ssize_t r;
                    while ((r = read(fd, buffer, BUFFER_SIZE)) > 0) {
                        send(client_fd, buffer, r, 0);
                    }
                    close(fd);
                }
            }
        }

        free(packet);
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", ip);
    }

    unlink(FILE_PATH);
    closelog();

    return 0;
}