
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static volatile sig_atomic_t shutdown_requested = 0;

static void signal_handler(int signal_number)
{
    (void)signal_number;
    shutdown_requested = 1;
}

static int install_signal_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        syslog(LOG_ERR, "Failed to install signal handlers: %s",
               strerror(errno));
        return -1;
    }

    return 0;
}

/* Send all bytes, handling partial sends and interruptions. */
static int send_all(int socket_fd, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        ssize_t result = send(socket_fd, data + sent,
                              length - sent, 0);

        if (result > 0) {
            sent += (size_t)result;
        } else if (result == -1 && errno == EINTR) {
            if (shutdown_requested) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    return 0;
}

/* Send the complete data file without loading it into memory. */
static int send_file(int socket_fd)
{
    char buffer[BUFFER_SIZE];
    int file_fd = open(DATA_FILE, O_RDONLY);

    if (file_fd == -1) {
        syslog(LOG_ERR, "Failed to open data file: %s",
               strerror(errno));
        return -1;
    }

    int status = 0;

    while (!shutdown_requested) {
        ssize_t bytes_read = read(file_fd, buffer, sizeof(buffer));

        if (bytes_read > 0) {
            if (send_all(socket_fd, buffer, (size_t)bytes_read) == -1) {
                status = -1;
                break;
            }
        } else if (bytes_read == 0) {
            break;
        } else if (errno == EINTR) {
            if (shutdown_requested) {
                status = -1;
                break;
            }
        } else {
            syslog(LOG_ERR, "Failed to read data file: %s",
                   strerror(errno));
            status = -1;
            break;
        }
    }

    if (close(file_fd) == -1) {
        syslog(LOG_ERR, "Failed to close data file: %s",
               strerror(errno));
        status = -1;
    }

    return status;
}

/* Append one complete newline-terminated packet to the data file. */
static int append_packet(int file_fd, const char *data, size_t length)
{
    size_t written = 0;

    while (written < length) {
        ssize_t result = write(file_fd, data + written,
                               length - written);

        if (result > 0) {
            written += (size_t)result;
        } else if (result == -1 && errno == EINTR) {
            if (shutdown_requested) {
                return -1;
            }
        } else {
            syslog(LOG_ERR, "Failed to write data file: %s",
                   strerror(errno));
            return -1;
        }
    }

    return 0;
}

/* Open, bind and listen before daemonizing. */
static int create_server_socket(void)
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd == -1) {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        return -1;
    }

    int reuse = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) == -1) {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(socket_fd);
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(socket_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(socket_fd);
        return -1;
    }

    if (listen(socket_fd, 10) == -1) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

static int run_as_daemon(void)
{
    pid_t pid = fork();

    if (pid == -1) {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        return -1;
    }

    if (pid > 0) {
        /* The parent exits after the child has been created. */
        _exit(EXIT_SUCCESS);
    }

    if (setsid() == -1) {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        return -1;
    }

    if (chdir("/") == -1) {
        syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
        return -1;
    }

    umask(0);

    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd == -1) {
        syslog(LOG_ERR, "Failed to open /dev/null: %s",
               strerror(errno));
        return -1;
    }

    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++) {
        if (dup2(null_fd, fd) == -1) {
            syslog(LOG_ERR, "dup2 failed: %s", strerror(errno));
            if (null_fd > STDERR_FILENO) {
                close(null_fd);
            }
            return -1;
        }
    }

    if (null_fd > STDERR_FILENO) {
        close(null_fd);
    }

    return 0;
}

static int handle_client(int client_fd, int file_fd)
{
    char received[BUFFER_SIZE];
    char *pending = NULL;
    size_t pending_length = 0;
    int status = 0;

    while (!shutdown_requested) {
        ssize_t bytes_received = recv(client_fd, received,
                                      sizeof(received), 0);

        if (bytes_received == 0) {
            break;
        }

        if (bytes_received == -1) {
            if (errno == EINTR && shutdown_requested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "recv failed: %s", strerror(errno));
            status = -1;
            break;
        }

        size_t new_length = pending_length + (size_t)bytes_received;
        char *new_pending = realloc(pending, new_length);

        if (new_pending == NULL) {
            syslog(LOG_ERR, "Failed to allocate packet buffer");
            status = -1;
            break;
        }

        pending = new_pending;
        memcpy(pending + pending_length, received,
               (size_t)bytes_received);
        pending_length = new_length;

        char *newline;
        while (!shutdown_requested &&
               (newline = memchr(pending, '\n', pending_length)) != NULL) {
            size_t packet_length =
                (size_t)(newline - pending) + 1;

            if (append_packet(file_fd, pending, packet_length) == -1) {
                status = -1;
                break;
            }

            /* Return all accumulated data after every complete packet. */
            if (send_file(client_fd) == -1) {
                status = -1;
                break;
            }

            pending_length -= packet_length;

            if (pending_length > 0) {
                memmove(pending, pending + packet_length,
                        pending_length);
            }
        }

        if (status == -1) {
            break;
        }
    }

    free(pending);
    return status;
}

int main(int argc, char *argv[])
{
    bool daemon_mode = false;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
        return EXIT_FAILURE;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (install_signal_handlers() == -1) {
        closelog();
        return -1;
    }

    int server_fd = create_server_socket();
    if (server_fd == -1) {
        closelog();
        return -1;
    }

    /* The listening socket is ready before the daemon forks. */
    if (daemon_mode && run_as_daemon() == -1) {
        close(server_fd);
        closelog();
        return EXIT_FAILURE;
    }

    int file_fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (file_fd == -1) {
        syslog(LOG_ERR, "Failed to open data file: %s",
               strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    while (!shutdown_requested) {
        struct sockaddr_in client_address;
        socklen_t address_length = sizeof(client_address);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_address,
                               &address_length);

        if (client_fd == -1) {
            if (errno == EINTR && shutdown_requested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &client_address.sin_addr,
                      client_ip, sizeof(client_ip)) == NULL) {
            snprintf(client_ip, sizeof(client_ip), "unknown");
        }

        syslog(LOG_INFO, "Connection accepted from %s", client_ip);

        (void)handle_client(client_fd, file_fd);

        if (close(client_fd) == -1) {
            syslog(LOG_ERR, "Failed to close client socket: %s",
                   strerror(errno));
        }

        syslog(LOG_INFO, "Connection closed from %s", client_ip);
    }

    if (shutdown_requested) {
        syslog(LOG_INFO, "Caught signal, exiting");
    }

    if (close(file_fd) == -1) {
        syslog(LOG_ERR, "Failed to close data file: %s",
               strerror(errno));
    }

    if (close(server_fd) == -1) {
        syslog(LOG_ERR, "Failed to close server socket: %s",
               strerror(errno));
    }

    if (unlink(DATA_FILE) == -1 && errno != ENOENT) {
        syslog(LOG_ERR, "Failed to remove data file: %s",
               strerror(errno));
    }

    closelog();
    return EXIT_SUCCESS;
}

