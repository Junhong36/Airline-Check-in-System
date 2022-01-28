/*
The goal is to simulate an airline check-in system, called ACS. 
The check-in system includes 2 queues and 5 clerks. One queue (Queue 0)
 for economy class and the other (Queue 1) for business class. 
 We assume that the customers in each queue are served FIFO, and 
 the customers in the business class have a higher priority than 
 the customers in the economy class. In other words, when a clerk 
 is available, the clerk picks a customer in the business class, 
 if any, to serve, and picks a customer in the economy class to 
 serve only if the business class queue is empty. When a clerk 
 is available and there is no customer in any queue, the clerk 
 remains idle. We assume that the service time for a customer is 
 known when the customer enters the system. You will use threads 
 to simulate the customers arriving and waiting for service, and 
 your program will schedule these customers following the above rules.
*/
#include <errno.h> // errno
#include <pthread.h>
#include <stdio.h>  // printf(), scanf(), setbuf(), perror()
#include <stdlib.h> // malloc()
#include <string.h> // define,string processing,etc
#include <sys/time.h>
#include <unistd.h>

#define MAX_CUSTOMER 50
#define MAX_LEN 20
#define MAX_CLERK 5

typedef struct customer_info customer_info_t;
struct customer_info { /// use this struct to record the customer information read from customers.txt
    int serve_flag; // when a customer is assigned a clerk, flag change to 1
    int user_id;
    int class_type;
    double arrival_time;
    double service_time;
};

void *queue_management(void *customer);
void *customer_entry(void *customer);
void heapSort(void *customer, int n);
void heapify(void *customer, int n, int i);
void swap(void *customer_a, void *customer_b);
double getCurrentTime();
int check_clerk();
void assign_clerk(int clerk_id);
void free_clerk(int clerk_id);

/* global variables */

struct timeval init_time; // use this variable to record the simulation start time;
                          // No need to use mutex_lock when reading this variable since the value would
                          // not be changed by thread once the initial time was set.

/* Other global variable may include: 
 1. condition_variables (and the corresponding mutex_lock) to represent each queue; 
 2. condition_variables to represent clerks
 3. others.. depend on your design
 */
pthread_cond_t customer_mutex[MAX_CUSTOMER]; //
pthread_mutex_t mutex1;
pthread_mutex_t time_mutex;
static int total;
int current_id = 0; // int clerks = 5, current_id = 0;
int clerks[MAX_CLERK];
int total_economy = 0, total_bussiness = 0;
double all_business_wait_time = 0, all_economy_wait_time = 0; // time

int main(int argc, char *argv[])
{

    // initialize all the condition variable and thread lock will be used
    FILE *fp = NULL;
    customer_info_t *customer_list[MAX_CUSTOMER];
    pthread_mutex_init(&mutex1, NULL);
    pthread_mutex_init(&time_mutex, NULL);
    pthread_t customId[MAX_CUSTOMER];      // customer thread id
    pthread_t management;
    int user_id, class_type, service_time, arrival_time, count = 0;

    gettimeofday(&init_time, NULL); // record start time

    /** Read customer information from txt file and store them in the structure you created 
		
	1. Allocate memory(array, link list etc.) to store the customer information.
	2. File operation: fopen fread getline/gets/fread ..., store information in data structure you created

	*/
    if (argc != 2) {
        fprintf(stderr, "Usage: ACS inputfile\n");
        exit(1);
    } else {

        if ((fp = fopen(argv[1], "r"))) {
            fscanf(fp, "%d\n ", &total);

            while ((fscanf(fp, "%d:%d,%d,%d\n ", &user_id, &class_type, &arrival_time, &service_time)) != EOF) {
                customer_info_t *new_customer = NULL;

                new_customer = (customer_info_t *)malloc(sizeof(customer_info_t));

                new_customer->serve_flag = 0;
                new_customer->user_id = user_id;
                new_customer->class_type = class_type;
                new_customer->arrival_time = arrival_time/10.00;
                new_customer->service_time = service_time/10.00;

                customer_list[count] = new_customer;
                count++;
            }

            fclose(fp);

        } else {
            fprintf(stderr, "File cannot open\n");
        }
    }

    // using heap sort to sort the customer
    // list base on arrival time.
    heapSort((void *)customer_list, total);

    // initialize clerks state
    for (int i = 0; i < MAX_CLERK; i++) {
        clerks[i] = 0;
    }
                
    // initialize conditional variables for all customers
    for (int i = 0; i < total; i++) { 
        pthread_cond_init(&customer_mutex[i], NULL);
    }

    // create customer thread
    // customer_list[i]: passing the customer information
    // (e.g., customer ID, arrival time, service time, etc.) to customer thread
    for (int i = 0; i < total; i++){
        pthread_create(&customId[i], NULL, customer_entry, (void *)customer_list[i]); 
    }

    // create queue management thread
    pthread_create(&management, NULL, queue_management, (void *)customer_list);

    // wait for all customer threads to terminate
    for (int i = 0; i < total; i++) {
        pthread_join(customId[i], NULL);
    }
    pthread_join(management, NULL);

    // destroy mutex & condition variable (optional)
    pthread_mutex_destroy(&mutex1);
    pthread_mutex_destroy(&time_mutex);

    for (int i = 0; i < total;i++){
        pthread_cond_destroy(&customer_mutex[i]);
    }
    // calculate the average waiting time of all customers
    printf("The average waiting time for all customers in the system is: %.2f seconds.\n",
           (all_business_wait_time + all_economy_wait_time) / (double)(total_bussiness + total_economy));
    printf("The average waiting time for all business-class customers is: %.2f seconds.\n",
           all_business_wait_time / (double)total_bussiness);
    printf("The average waiting time for all economy-class customers is: %.2f seconds.\n",
           all_economy_wait_time / (double)total_economy);

    // free malloc memory
    for (int i = 0; i < total; i++) {
        free(customer_list[i]);
    }

    return 0;
}

void quick_sort(void *customer, int L, int R)
{
    int p;
    customer_info_t *temp = (customer_info_t *)malloc(sizeof(customer_info_t));
    customer_info_t **customer_list = (customer_info_t **)customer;

    temp->serve_flag = 0;
    temp->user_id = 0;
    temp->class_type = 0;
    temp->arrival_time = 0;
    temp->service_time = 0;

    if(L >= R){
        free(temp);
        return;
    }

    p = L;

    for (int i = L + 1; i < R; i++) {
        if(customer_list[i]->arrival_time < customer_list[L]->arrival_time){
            p++;
            temp->serve_flag = customer_list[i]->serve_flag;
            temp->user_id = customer_list[i]->user_id;
            temp->class_type = customer_list[i]->class_type;
            temp->arrival_time = customer_list[i]->arrival_time;
            temp->service_time = customer_list[i]->service_time;

            customer_list[i]->serve_flag = customer_list[p]->serve_flag;
            customer_list[i]->user_id = customer_list[p]->user_id;
            customer_list[i]->class_type = customer_list[p]->class_type;
            customer_list[i]->arrival_time = customer_list[p]->arrival_time;
            customer_list[i]->service_time = customer_list[p]->service_time;

            customer_list[p]->serve_flag = temp->serve_flag;
            customer_list[p]->user_id = temp->user_id;
            customer_list[p]->class_type = temp->class_type;
            customer_list[p]->arrival_time = temp->arrival_time;
            customer_list[p]->service_time = temp->service_time;
        }
    }

    temp->serve_flag = customer_list[L]->serve_flag;
    temp->user_id = customer_list[L]->user_id;
    temp->class_type = customer_list[L]->class_type;
    temp->arrival_time = customer_list[L]->arrival_time;
    temp->service_time = customer_list[L]->service_time;

    customer_list[L]->serve_flag = customer_list[p]->serve_flag;
    customer_list[L]->user_id = customer_list[p]->user_id;
    customer_list[L]->class_type = customer_list[p]->class_type;
    customer_list[L]->arrival_time = customer_list[p]->arrival_time;
    customer_list[L]->service_time = customer_list[p]->service_time;

    customer_list[p]->serve_flag = temp->serve_flag;
    customer_list[p]->user_id = temp->user_id;
    customer_list[p]->class_type = temp->class_type;
    customer_list[p]->arrival_time = temp->arrival_time;
    customer_list[p]->service_time = temp->service_time;
    quick_sort((void *)customer_list, L, p - 1);
    quick_sort((void *)customer_list, p + 1, R);
}

// Function to swap the the position of two elements
void swap(void *a, void *b)
{
    customer_info_t *customer_a = (customer_info_t *)a;
    customer_info_t *customer_b = (customer_info_t *)b;

    customer_info_t *temp = (customer_info_t *)malloc(sizeof(customer_info_t));

    temp->serve_flag = customer_a->serve_flag;
    temp->user_id = customer_a->user_id;
    temp->class_type = customer_a->class_type;
    temp->arrival_time = customer_a->arrival_time;
    temp->service_time = customer_a->service_time;

    customer_a->serve_flag = customer_b->serve_flag;
    customer_a->user_id = customer_b->user_id;
    customer_a->class_type = customer_b->class_type;
    customer_a->arrival_time = customer_b->arrival_time;
    customer_a->service_time = customer_b->service_time;

    customer_b->serve_flag = temp->serve_flag;
    customer_b->user_id = temp->user_id;
    customer_b->class_type = temp->class_type;
    customer_b->arrival_time = temp->arrival_time;
    customer_b->service_time = temp->service_time;

    free(temp);
}

void heapify(void *customer, int n, int i)
{
    customer_info_t **customer_list = (customer_info_t **)customer;

    // Find largest among root, left child and right child
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < n && customer_list[left]->arrival_time > customer_list[largest]->arrival_time)
        largest = left;

    if (right < n && customer_list[right]->arrival_time > customer_list[largest]->arrival_time)
        largest = right;

    // Swap and continue heapifying if root is not largest
    if (largest != i) {
        swap((void *)customer_list[i], (void *)customer_list[largest]);
        heapify((void *)customer_list, n, largest);
    }
}

// Main function to do heap sort
void heapSort(void *customer, int n)
{
    customer_info_t **customer_list = (customer_info_t **)customer;

    // Build max heap
    for (int i = n / 2 - 1; i >= 0; i--)
        heapify((void *)customer_list, n, i);

    // Heap sort
    for (int i = n - 1; i >= 0; i--) {
        swap((void *)customer_list[0], (void *)customer_list[i]);

        // Heapify root element to get highest element at root again
        heapify((void *)customer_list, i, 0);
    }
}

// check which clerk is available 
int check_clerk(){
    for (int i = 0; i < MAX_CLERK; i++) {
        if (clerks[i] == 0) {
            return (i + 1);
        }
    }
    return -1;
}

void assign_clerk(int clerk_id){
    clerks[clerk_id] = 1; // the clerk is now serving
}

void free_clerk(int clerk_id){
    clerks[clerk_id] = 0; // the clerk is now available
}

// To manage the arrival customers and assign clerks to each waiting customer
void *queue_management(void *customer)
{
    customer_info_t **customer_list = (customer_info_t **)customer;

    customer_info_t *business_list[MAX_CUSTOMER];
    customer_info_t *economy_list[MAX_CUSTOMER];
    int business_head = 0, business_tail = 0;
    int economy_head = 0, economy_tail = 0;
    double current_time = 0;
    int count1 = 0; // count how many customers already added to queue
    int count2 = 0; // count how many customers already assigned a clerk

    while (count2 != total) { // count2 != total

        current_time = getCurrentTime();
        // add arrival customers to waiting list
        for (int i = count1; i < total; i++) {
            if ((customer_list[i]->arrival_time) <= current_time) {
                count1++;
                if (customer_list[i]->class_type == 0) {
                    economy_list[economy_tail] = customer_list[i];
                    fprintf(stdout, "A customer arrives: customer ID %2d. \n", customer_list[i]->user_id);
                    economy_tail++;
                    total_economy++;
                    fprintf(stdout, "A customer enters a queue: the queue ID 0, and length of the queue %2d. \n",
                            (economy_tail - economy_head));
                } 
                else {
                    business_list[business_tail] = customer_list[i];
                    fprintf(stdout, "A customer arrives: customer ID %2d. \n", customer_list[i]->user_id);
                    business_tail++;
                    total_bussiness++;
                    fprintf(stdout, "A customer enters a queue: the queue ID 1, and length of the queue %2d. \n",
                            (business_tail - business_head));
                }
            }
            else{
                break;
            }
        }

        // assign assign clerks to each waiting customer
        for (int i = check_clerk(); (business_head < business_tail) && (i > 0); business_head++, i = check_clerk()) {
            pthread_mutex_lock(&mutex1); // lock the mutex1
            {
                current_id = business_list[business_head]->user_id;
                business_list[business_head]->serve_flag = i;
                assign_clerk(i - 1);
                count2++;
                pthread_cond_signal(&customer_mutex[current_id - 1]);
            }
            pthread_mutex_unlock(&mutex1); // unlock the mutex1
        }

        for (int i = check_clerk(); (economy_head < economy_tail) && (i > 0); economy_head++, i = check_clerk()) {
            pthread_mutex_lock(&mutex1); // lock the mutex1
            {
                current_id = economy_list[economy_head]->user_id;
                economy_list[economy_head]->serve_flag = i;
                assign_clerk(i - 1);
                count2++;
                pthread_cond_signal(&customer_mutex[current_id - 1]);
            }
            pthread_mutex_unlock(&mutex1); // unlock the mutex1
        }
    }

    return NULL;
}

// the customer
void *customer_entry(void *customer)
{

    customer_info_t *curr_customer = (customer_info_t *)customer;

    int clerk_id = 0;
    double wait_time = 0, current_time = 0;

    pthread_mutex_lock(&mutex1); // lock the mutex1
    {
        while (!(curr_customer->serve_flag)) {  // wait for a clerk
            pthread_cond_wait(&customer_mutex[(curr_customer->user_id) - 1], &mutex1);
        }

        clerk_id = curr_customer->serve_flag; // know clerk id that is serving 
    }
    pthread_mutex_unlock(&mutex1); // unlock the mutex1

    current_time = getCurrentTime();
    wait_time = current_time - curr_customer->arrival_time;

    pthread_mutex_lock(&time_mutex); // lock the time_mutex
    {
        if (curr_customer->class_type == 0) {  // count up the wait time
            all_economy_wait_time += wait_time;
        } else {
            all_business_wait_time += wait_time;
        }
    }
    pthread_mutex_unlock(&time_mutex); // unlock the time_mutex
    fprintf(stdout, "A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n",
            current_time, curr_customer->user_id, clerk_id);

    usleep((curr_customer->service_time) * 1000000); // serve time

    current_time = getCurrentTime();
    fprintf(stdout, "A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n",
            current_time, curr_customer->user_id, clerk_id);

    pthread_mutex_lock(&mutex1); // lock the mutex1
    {
        free_clerk(clerk_id - 1); // free the clerk
    }
    pthread_mutex_unlock(&mutex1); // unlock the mutex1

    return NULL;
}

// use to obtain the current machine time
double getCurrentTime()
{
    struct timeval cur_time;
    double cur_secs, init_secs;

    init_secs = (init_time.tv_sec + (double)init_time.tv_usec / 1000000);

    gettimeofday(&cur_time, NULL);
    cur_secs = (cur_time.tv_sec + (double)cur_time.tv_usec / 1000000);

    return (cur_secs - init_secs);
}