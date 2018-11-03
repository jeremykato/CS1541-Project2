#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <string.h>
#include "CPU.h"
#include "cache.h"
#include "queue.h"


int main(void)
{
	struct queue write_buffer;
	initialize_queue(&write_buffer, 20);

	enqueue(&write_buffer, 10);
	enqueue(&write_buffer, 12);
	enqueue(&write_buffer, 14);
	enqueue(&write_buffer, 16);
	enqueue(&write_buffer, 22);
	enqueue(&write_buffer, 100);
	enqueue(&write_buffer, 777);
	dequeue(&write_buffer);
	dequeue(&write_buffer);
	dequeue(&write_buffer);
	dequeue(&write_buffer);

	if(contains(&write_buffer, 10))
		printf("Contains 10\n");
	else
		printf("Does not contain 10\n");
	if(contains(&write_buffer, 12))
		printf("Contains 12\n");
	else
		printf("Does not contain 12\n");
	if(contains(&write_buffer, 13))
		printf("Contains 13\n");
	else
		printf("Does not contain 13\n");
	if(contains(&write_buffer, 14))
		printf("Contains 14\n");
	else
		printf("Does not contain 14\n");
	if(contains(&write_buffer, 15))
		printf("Contains 15\n");
	else
		printf("Does not contain 15\n");
	if(contains(&write_buffer, 16))
		printf("Contains 16\n");
	else
		printf("Does not contain 16\n");
	if(contains(&write_buffer, 22))
		printf("Contains 22\n");
	else
		printf("Does not contain 12\n");
}