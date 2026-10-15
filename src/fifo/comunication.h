#ifndef FIFO_COMUNICATION_H
#define FIFO_COMUNICATION_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CAPACITY 32768

#define FIFO_REQUEST "/tmp/practica1_fifo_request"
#define FIFO_RESPONSE "/tmp/practica1_fifo_response"

#define CRIT_EXIT     0
#define CRIT_TRACK_ID 1
#define CRIT_GENRE    2
#define CRIT_LABEL    3

typedef struct {
    int criteria;
    char query[MAX_CAPACITY];
} Request;

typedef struct {
    int status;  // 0 success, 1 failure, 2 server shutdown acknowledgment
    char result[MAX_CAPACITY];
} Response;

static ssize_t read_full(int fd, void *buffer, size_t size) {
    char *p = buffer;
    size_t total = 0;

    while (total < size) {
        ssize_t n = read(fd, p + total, size - total);
        if (n == 0) return (ssize_t)total;
        total += (size_t)n;
    }

    return (ssize_t)total;
}

static ssize_t write_full(int fd, const void *buffer, size_t size) {
    const char *p = buffer;
    size_t total = 0;

    while (total < size) {
        ssize_t n = write(fd, p + total, size - total);
        total += (size_t)n;
    }

    return (ssize_t)total;
}

#endif
