#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAX_CAPACITY  32768

#define SHM_KEY   0x1234
#define SEM_REQ   "/p1_req"
#define SEM_RESP  "/p1_resp"

#define CRIT_EXIT     0     // criteria = 0 => orden de terminación
#define CRIT_TRACK_ID 1
#define CRIT_GENRE    2
#define CRIT_LABEL    3

typedef struct {
    int criteria;
    char query[MAX_CAPACITY];
} Request;

typedef struct {
    int status;  // 0 success, 1 failure
    char result[MAX_CAPACITY];
} Response;

typedef struct {
    pid_t    server_pid; // servidor activo
    pid_t    client_pid; // cliente activo
    Request  req;
    Response resp;
} SharedData;

#endif