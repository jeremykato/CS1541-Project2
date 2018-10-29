/*REPURPOSED QUEUE CODE FROM MY CS1550 PROJECT*/
/*USE THIS QUEUE FOR TO STORE REPRESENT THE WRITE BUFFER*/

//simple queue holds car information
struct queue {
	int size;
	int capacity;
	int front;
	int back;
	int *elements;
};

//simple queue interface methods
void initialize_queue(struct queue *q, int queue_capacity);
int enqueue(struct queue *q, int element);
int dequeue(struct queue *q);
int peek(struct queue *q);

//make an empty queue and return a pointer to it
void initialize_queue(struct queue *q, int queue_capacity)
{
	q->capacity = queue_capacity;
	q->size = 0;
	q->front = 0;
	q->back = INVALID;
	q->elements = mmap(NULL, sizeof(int) * queue_capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
}

//add an element to the back of the queue
int enqueue(struct queue *q, int element)
{
	if(q->size == q->capacity)
		return FALSE;
	(q->size)++;
	(q->back) = (q->back + 1) % q->capacity;
	q->elements[q->back] = element;
	return TRUE;
}

//remove an element from the rear of the queue
//returns element if possible, otherwise -1
int dequeue(struct queue *q)
{
	if(q->size <= 0)
		return -1;
	int element = q->elements[q->front];
	(q->size)--;
	(q->front) = (q->front + 1) % q->capacity;
	return element;
}

//get a reference to the front of the queue without dequeueing it
int peek(struct queue *q)
{
	return q->elements[q->front];
}