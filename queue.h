/*REPURPOSED QUEUE CODE FROM MY CS1550 PROJECT*/
/*USE THIS QUEUE TO REPRESENT THE WRITE BUFFER*/

#include <sys/mman.h>

//simple queue holds car information
struct zqueue {
	int size;
	int capacity;
	int front;
	int back;
	int *elements;
};

//simple queue interface methods
void initialize_queue(struct zqueue *q, int queue_capacity);
int enqueue(struct zqueue *q, int element);
int dequeue(struct zqueue *q);
int peek(struct zqueue *q);

//make an empty queue and return a pointer to it
void initialize_queue(struct zqueue *q, int queue_capacity)
{
	q->capacity = queue_capacity;
	q->size = 0;
	q->front = 0;
	q->back = -1;
	q->elements = mmap(NULL, sizeof(int) * queue_capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
}

//add an element to the back of the queue
int enqueue(struct zqueue *q, int element)
{
	if(q->size == q->capacity)
		return 0;
	(q->size)++;
	(q->back) = (q->back + 1) % q->capacity;
	q->elements[q->back] = element;
	return 1;
}

//remove an element from the rear of the queue
//returns element if possible, otherwise -1
int dequeue(struct zqueue *q)
{
	if(q->size <= 0)
		return -1;
	int element = q->elements[q->front];
	(q->size)--;
	(q->front) = (q->front + 1) % q->capacity;
	return element;
}

//get a reference to the front of the queue without dequeueing it
int peek(struct zqueue *q)
{
	return q->elements[q->front];
}

//returns 1 if the queue contains the value in question, otherwise 0
//performs a sequential search
int contains(struct zqueue *q, int value)
{
	int j = q->front; //physical index
	for(int i = 0; i < q->size; i++) //logical index
	{
		if(q->elements[j] == value)
		{
			return 1;
		}
		j = (j + 1) % q->capacity;
	}
	return 0;
}