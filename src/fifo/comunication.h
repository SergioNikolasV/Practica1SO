#ifndef COMMON_H
#define COMMON_H

#define maxCapacity  32768

typedef struct {
    int criteria;
    char query[maxCapacity];
} Request;

typedef struct {
    int status;  // 0 success, 1 failure
    char result[maxCapacity];
} Response;

#endif