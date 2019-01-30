#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/*
 * Resources occupation: 
 *     North: 0, 1.
 *     East:  1, 2.
 *     South: 2, 3.
 *     West:  3, 0.
 */
enum Direction
{
	queueNorth = 0,
	queueEast,
	queueSouth,
	queueWest
};

/*
 * Store the data of car.
 *     car_no: ID of the car.
 *     car_direction: The direction that the car comes from.
 *     thread_car: The thread of the car.
 *     next_car: The pointer points to the next car.
 */
struct car
{
	int car_no;
	enum Direction car_direction;
	pthread_t thread_car;
	struct car *next_car;
};

/* Count the number of threads that is waiting to pass through the crossroads. */
int thread_count;

/* Store the state if the cars on the direction need to wait or not */
int is_wait[4];

/* The head of the linked list that store the all the data of cars. */
struct car *car_head;

/* The thread of deadlock. */
pthread_t thread_deadlock;

/* The attribute of threads. */
pthread_attr_t thread_attr;

/* The condition used to check if the car needs waiting. */
pthread_cond_t thread_cond[4];

/* The mutex of queue of cars. */
pthread_mutex_t thread_mutex_car_queue[4];

/* The mutex of resources. */
pthread_mutex_t thread_mutex_resources[4];

/* The mutex of deadlock. */
pthread_mutex_t thread_mutex_deadlock;

/* The scheduler to solve the BAT problem. 
 * @param car_pointer: The data of a car. 
 */
void *Scheduler(void *car_pointer)
{
	struct car *car_pass = (struct car *)car_pointer;
	int direction = (int)(car_pass->car_direction);
	int no = car_pass->car_no;

	/* Wait operation. */
	// Lock the mutex of the queue of cars.
	pthread_mutex_lock(&thread_mutex_car_queue[direction]);

	// If there is the direction is set to be waiting, keeps it waiting.
	while (is_wait[direction])
	{
		pthread_cond_wait(&thread_cond[direction], &thread_mutex_car_queue[direction]);
	}

	// Set the direction to be waiting.
	__sync_fetch_and_or(&is_wait[direction], 1);

	// Unlock the mutex of the queue of cars.
	pthread_mutex_unlock(&thread_mutex_car_queue[direction]);

	// Show the arriving info of the car.
	switch (direction)
	{
	case 0:
		printf("Car<%d> arrives from North.\n", no);
		break;
	case 1:
		printf("Car<%d> arrives from East.\n", no);
		break;
	case 2:
		printf("Car<%d> arrives from South.\n", no);
		break;
	case 3:
		printf("Car<%d> arrives from West.\n", no);
		break;
	}

	// Lock the occupied resources.
	pthread_mutex_lock(&thread_mutex_resources[direction]);
	pthread_mutex_lock(&thread_mutex_resources[(direction + 1) % 4]);

	// Lock the mutex of the queue of cars.
	pthread_mutex_lock(&thread_mutex_car_queue[direction]);

	// Set the direction to be NOT waiting.
	__sync_fetch_and_and(&is_wait[direction], 0);

	/* Signal operation. */
	// Awake the thread.
	pthread_cond_signal(&thread_cond[direction]);

	// Unlock the mutex of the queue of cars.
	pthread_mutex_unlock(&thread_mutex_car_queue[direction]);

	// Delay for one second.
	sleep(1);

	// Unlock the occupied resources.
	pthread_mutex_unlock(&thread_mutex_resources[direction]);
	pthread_mutex_unlock(&thread_mutex_resources[(direction + 1) % 4]);

	// The number of threads minus one atomically, since the car just left.
	__sync_fetch_and_sub(&thread_count, 1);

	// Show the leaving info of the car.
	switch (direction)
	{
	case 0:
		printf("Car<%d> leaves from North.\n", no);
		break;
	case 1:
		printf("Car<%d> leaves from East.\n", no);
		break;
	case 2:
		printf("Car<%d> leaves from South.\n", no);
		break;
	case 3:
		printf("Car<%d> leaves from West.\n", no);
		break;
	}

	// Exit the thread.
	pthread_exit(NULL);
}

/*
 * Deadlock detector.
 * @param arg: It is useless here.
 */
void *DeadlockDetect(void *arg)
{
	// Set 0 to be non-deadlocked, and 1 to be deadlocked.
	int check_waiting = 0;

	// If there is a thread in the queue, this function have to work.
	while (thread_count > 0)
	{
		// Lock the mutex of deadlock.
		pthread_mutex_lock(&thread_mutex_deadlock);

		// Check if there is a direction that is in waiting state.
		int is_waiting = is_wait[0] * is_wait[1] * is_wait[2] * is_wait[3];

		// Unlock the mutex of deadlock.
		pthread_mutex_unlock(&thread_mutex_deadlock);

		if ((is_waiting) && (check_waiting == 0))
		{
			// Set to 1 atomically; and it means that a deadlock happens.
			__sync_fetch_and_or(&check_waiting, 1);
			printf("Deadlock detected.\n");
		}
		else if (is_waiting == 0)
		{
			// Set to 0 atomically.
			__sync_fetch_and_and(&check_waiting, 0);
		}
	}

	// Exit the thread.
	pthread_exit(NULL);
}

/*
 * Check if the user's input is correct.
 * @param carQueue: The user's input string. 
 */
char *CarQueueCheck(char *carQueue)
{
	// The lowerclass letters coressponding to North, East, South, West.
	char correct_upper[4] = {
		'N',
		'E',
		'S',
		'W'};
	// The upperclass letters coressponding to North, East, South, West.
	char correct_lower[4] = {
		'n',
		'e',
		's',
		'w'};

	// The number of cars.
	int len_carQueue = strlen(carQueue);
	char *converted_carQueue = NULL;

	// Use to store the standardized string.
	converted_carQueue = (char *)malloc(len_carQueue * sizeof(char));

	// Fail to allocate the memory, so this program exits with error state.
	if (NULL == converted_carQueue)
	{
		printf("\n------------------------------------------------------------\n");
		printf("|            Error: Fail to allocate the memory.           |\n");
		printf("------------------------------------------------------------\n");
		exit(1);
	}

	int i, j;
	int flag;
	for (i = 0; i < len_carQueue; i++)
	{
		flag = 0;
		for (j = 0; j < 4; j++)
		{
			// If the character is upperclass, convert it to lowerclass;
			// else if the character is lowerclass, do nothing;
			// else, the character cannot be recognized.
			if (carQueue[i] == correct_upper[j])
			{
				converted_carQueue[i] = (char)(carQueue[i] + ('a' - 'A'));
				flag = 1;
				break;
			}
			else if (carQueue[i] == correct_lower[j])
			{
				converted_carQueue[i] = carQueue[i];
				flag = 1;
				break;
			}
		}

		// The carQueue contains incorrect direction, so this program exits with error state.
		if (flag == 0)
		{
			printf("\n------------------------------------------------------------\n");
			printf("|    Error: The <carQueue> contains incorrect direction.   |\n");
			printf("------------------------------------------------------------\n");
			exit(1);
		}
	}

	return converted_carQueue;
}

/*
 * Initialize the mutex, condition, linked list, and so on.
 * @param: argc: The number of input arguments.
 *         argv: The user's input string.
 */
void Initialize(int argc, char **argv)
{
	// The number of arguments is incorrect, so this program exits with error state.
	if (argc != 2)
	{
		printf("\n------------------------------------------------------------\n");
		printf("|       Error: The number of arguments is incorrect.       |\n");
		printf("------------------------------------------------------------\n");
		exit(1);
	}

	// Standardize the carQueue.
	char *converted_carQueue = CarQueueCheck(argv[1]);

	printf("\n------------------------------------------------------------\n");
	printf("|                   <carQueue> you input:                  |\n");
	printf("------------------------------------------------------------\n");
	int i;
	for (i = 0; i < strlen(converted_carQueue); i++)
	{
		printf("<%c>%s", converted_carQueue[i],
			   ((i == strlen(converted_carQueue) - 1) ? "\n" : ", "));
	}
	printf("------------------------------------------------------------\n");

	/* Store the number and direction by linked list. */
	struct car *car_tail, *car_new;
	car_head = (struct car *)malloc(sizeof(struct car));
	car_head->next_car = NULL;
	car_tail = car_head;

	for (i = 0; i < strlen(converted_carQueue); i++)
	{
		car_new = (struct car *)malloc(sizeof(struct car));
		car_new->car_no = i + 1;
		car_new->next_car = NULL;

		// Car direction.
		switch (converted_carQueue[i])
		{
		case 'n':
			car_new->car_direction = queueNorth;
			break;
		case 'e':
			car_new->car_direction = queueEast;
			break;
		case 's':
			car_new->car_direction = queueSouth;
			break;
		case 'w':
			car_new->car_direction = queueWest;
			break;
		}

		car_tail->next_car = car_new;
		car_tail = car_new;
	}

	// The number of threads begins with the number of the queue of cars.
	thread_count = strlen(converted_carQueue);

	// Check if there is any car waiting;
	// Set 0 if no car is waiting; 1, otherwise.
	for (i = 0; i < 4; i++)
	{
		is_wait[i] = 0;
	}

	// Initialize the attibute of threads.
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

	// Initialize the condition of threads.
	for (i = 0; i < 4; i++)
	{
		pthread_cond_init(&thread_cond[i], NULL);
	}

	// Initialize the mutex of threads.
	pthread_mutex_init(&thread_mutex_deadlock, NULL);
	for (i = 0; i < 4; i++)
	{
		pthread_mutex_init(&thread_mutex_resources[i], NULL);
		pthread_mutex_init(&thread_mutex_car_queue[i], NULL);
	}

	// Create the thread of deadlock.
	int res;
	res = pthread_create(&thread_deadlock, &thread_attr, DeadlockDetect, NULL);
	if (res != 0)
	{
		printf("\n------------------------------------------------------------\n");
		printf("|      Error: Some thread is NOT created successfully.     |\n");
		printf("------------------------------------------------------------\n");

		exit(1);
	}

	printf("\n------------------------------------------------------------\n");
	printf("|                         Run-down                         |\n");
	printf("------------------------------------------------------------\n");

	/* Traverse the linked list and create threads. */
	struct car *car_pointer;
	car_pointer = car_head->next_car;
	while (car_pointer != NULL)
	{
		res = pthread_create(&car_pointer->thread_car, &thread_attr, Scheduler,
							 (void *)car_pointer);

		// Check if the thread is created successfully.
		if (res != 0)
		{
			printf("\n------------------------------------------------------------\n");
			printf("|      Error: Some thread is NOT created successfully.     |\n");
			printf("------------------------------------------------------------\n");

			exit(1);
		}

		car_pointer = car_pointer->next_car;
	}

	/* Free the memory of pointers. */
	car_tail = NULL;
	car_new = NULL;
	car_pointer = NULL;

	free(car_tail);
	free(car_new);
	free(car_pointer);

	return;
}

/*
 * The easy user interface.
 * @param: void.
 */
void UserInterface(void)
{
	printf("------------------------------------------------------------\n\n");
	printf("Format: ./<Filename> <carQueue>\n");
	printf("Notice: <carQueue> can ONLY contains <n> <e> <s> <w>, \n");
	printf("        which means North, East, South, West, separately.\n");
	printf("Example: ./test nsewwesn\n");
	printf("        It means that the incoming car queue is: \n");
	printf("        <n> <s> <e> <w> <w> <e> <s> <n>\n\n");
	printf("------------------------------------------------------------\n");

	return;
}

/*
 * The main function.
 * @param: argc: The number of input arguments.
 *         argv: The user's input string.
 */
int main(int argc, char **argv)
{
	UserInterface();
	Initialize(argc, argv);

	/* Wait the threads to finish. */
	struct car *car_pointer = NULL;
	car_pointer = car_head->next_car;
	while (car_pointer != NULL)
	{
		pthread_join(car_pointer->thread_car, NULL);
		car_pointer = car_pointer->next_car;
	}

	printf("------------------------------------------------------------\n\n");

	/* Destroy mutex and exit the thread. */
	pthread_attr_destroy(&thread_attr);
	pthread_mutex_destroy(&thread_mutex_deadlock);
	int i;
	for (i = 0; i < 4; i++)
	{
		pthread_mutex_destroy(&thread_mutex_resources[i]);
		pthread_mutex_destroy(&thread_mutex_car_queue[i]);
		pthread_cond_destroy(&thread_cond[i]);
	}

	// Exit the thread.
	pthread_exit(NULL);

	return 0;
}
