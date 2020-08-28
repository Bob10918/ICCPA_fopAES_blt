/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: bob
 *
 * Created on 19 agosto 2020, 10.51
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

#define TRUE 0xff
#define FALSE 0
#define BYTE_SPACE 256

#define KEY_SIZE 16     //key size in byte
#define KEY_SIZE_INT KEY_SIZE/(sizeof(int)/sizeof(char))    //key size in integers

typedef struct Relation_s{
    int in_relation_with;
    char value;
    struct Relation_s* next;
}Relation;  //controllare se struct sono puntatori

typedef struct Subkey_element_s{
    char subkeys[BYTE_SPACE][KEY_SIZE];
    struct Subkey_element_s* next;
}Subkey_element;

//float T[N][l*KEY_SIZE], char m[KEY_SIZE], Relation* relations[KEY_SIZE]
typedef struct {
    float** T;
    char* m;
    Relation** relations;
}Thread_args;

int l;        //number of points acquired per instruction processing
//float T[M][N][l*KEY_SIZE];      //array containing all the power traces (divided per messages)
int N;        //number of power traces captured from a device processing N encryptions of the same message
//char m[M][KEY_SIZE];
int M;        //number of plaintext messages
float threshold;    //threshold for collision determination

int max_threads;    //max number of threads to run simultaneously
int threads_number = 0;
pthread_mutex_t mutex_threads_number = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_relations = PTHREAD_MUTEX_INITIALIZER;

void get_relations(float T[M][N][l*KEY_SIZE], char m[M][KEY_SIZE], Relation** relations);
void* find_collisions(void* args);
void guess_key(Relation** relations);
float standard_deviation(float T[N][l*KEY_SIZE], int theta, int t);
//float covariance(float T[N][l*KEY_SIZE], int theta0, int theta1, int t);
float covariance(float T[N][l*KEY_SIZE], int theta0, int theta1, int t, float* sum_array);
void pearson(float T[N][l*KEY_SIZE], int theta0, int theta1, float* correlation);
float optimized_pearson(float T[N][l*KEY_SIZE], int theta0, int theta1, float* sum_array, float* std_dev_array);
Subkey_element* guess_subkey(int to_guess, Relation** relations, int* guessed);
void resolve_relations(int start, Relation** relations, char* new_guessed, char* xor_array);
void combine_subkeys(Subkey_element* subkeys_list, int* partial_key);

void create_params(float T[M][N][l*KEY_SIZE], char m[M][KEY_SIZE]){
    int i,j,k, h;
    for(i=0; i<M; i++){
        for(j=0; j<N; j++){
            for(k=0; k<KEY_SIZE; k++){
                for(h=0; h<l; h++){
                    T[i][j][k*l+h] = h;
                }
            }
        }
        for(j=0; j<KEY_SIZE; j++){
            m[i][j] = (char) j;
        }
    }
}

int main(int argc, char** argv) {
    /*
    //da capire bene come vengono passati i parametri
    l = argv[1]; 
    //power traces are provided as array of integers (each int is the measured power in the correspondant instant)
    T = argv[2];    
    N = sizeof(T[0])/sizeof(T[0][0]);     //controllare se funziona
    char m[M][KEY_SIZE] = argv[3];     //array containing all the plaintext messages
    M = sizeof(m)/sizeof(m[0]);
    //TODO controllare se l divide N?
    threshold = argv[4];
    */
    
    
    l = 3;
    N = 5;
    M = 3;
    threshold = 0.9;
    max_threads = 4;
    float T[M][N][l*KEY_SIZE];
    char m[M][KEY_SIZE];
    create_params(T, m);
    
    Relation* relations[KEY_SIZE];
    
    get_relations(T, m, relations);
    guess_key(relations);
    
    //free memory allocated for relations lists
    Relation* pointer_relations_current;
    Relation* pointer_relations_next;
    for(int i=0; i<KEY_SIZE; i++){
        pointer_relations_current = relations[i];
        if(pointer_relations_current!=NULL){
            pointer_relations_next = pointer_relations_current->next;
            while(pointer_relations_next!=NULL){
                free(pointer_relations_current);
                pointer_relations_current = pointer_relations_next;
                pointer_relations_next = pointer_relations_current->next;
            }
            free(pointer_relations_current);
        }
    }
    
    //free mutexes
    pthread_mutex_destroy(&mutex_relations);
    pthread_mutex_destroy(&mutex_threads_number);
    
    return (EXIT_SUCCESS);
}

//decision function returning true or false depending on whether the same data was involved in two given instructions theta0 and theta1
//compare the value of a synthetic criterion with a practically determined threshold
//as criterion we used the maximum Pearson correlation factor
//theta0 and theta1 are time instant at which instruction starts (for performance reasons)
int collision(float T[N][l*KEY_SIZE], int theta0, int theta1, float* sum_array, float* std_dev_array){
    float correlation[l];
    //pearson(T, theta0, theta1, correlation);
    float max_correlation = optimized_pearson(T, theta0, theta1, sum_array, std_dev_array);
    
    //check if max is greater than threshold
    if(max_correlation>threshold){
        return TRUE;
    }
    else{
        return FALSE;
    }
}

void pearson(float T[N][l*KEY_SIZE], int theta0, int theta1, float* correlation){
    int i;
    for(i=0; i<l; i++){
//        correlation[i] = covariance(T, theta0, theta1, i)/(standard_deviation(T, theta0, i)*standard_deviation(T, theta1, i));
    }
}

//efficiently compute in one step both sum and std deviation
void compute_arrays(float T[N][l*KEY_SIZE], float* sum_array, float* std_dev_array){
    float sum;
    float squared_sum;
    for(int i=0; i<(l*KEY_SIZE); i++){
        sum = 0;
        squared_sum = 0;
        for(int j=0; j<N; j++){
            sum += T[j][i];
            squared_sum += T[j][i]*T[j][i];
        }
        sum_array[i] = sum;
        std_dev_array[i] = sqrtf((N*squared_sum)-(sum*sum));
    }
}

float optimized_pearson(float T[N][l*KEY_SIZE], int theta0, int theta1, float* sum_array, float* std_dev_array){
    int i;
    float correlation;
    float max_correlation = 0;
    for(i=0; i<l; i++){
        //correlation[i] = covariance(T, theta0, theta1, i)/(standard_deviation(T, theta0, i)/standard_deviation(T, theta1, i));
        correlation = covariance(T, theta0, theta1, i, sum_array)  /  (std_dev_array[theta0+i] * std_dev_array[theta1+i]);
        if(correlation > max_correlation){
            max_correlation = correlation;
        }
    }
    return max_correlation;
}

/*
float covariance(float T[N][l*KEY_SIZE], int theta0, int theta1, int t){
    int i;
    float first_sum=0, second_sum=0, third_sum=0;
    for(i=0; i<N; i++){
        first_sum += (T[i][theta0+t]*T[i][theta1+t]);
        second_sum += T[i][theta0+t];
        third_sum += T[i][theta1+t];
    }
    return (N*first_sum)-(second_sum*third_sum);
}
 * */

//for optimized pearson
float covariance(float T[N][l*KEY_SIZE], int theta0, int theta1, int t, float* sum_array){
    int i;
    float first_sum=0;
    for(i=0; i<N; i++){
        first_sum += (T[i][theta0+t]*T[i][theta1+t]);
    }
    return (N*first_sum)-(sum_array[theta0+t]*sum_array[theta1+t]);
}

float standard_deviation(float T[N][l*KEY_SIZE], int theta, int t){
    int i;
    float first_sum=0, second_sum=0;
    for(i=0; i<N; i++){
        first_sum += (T[i][theta+t])*(T[i][theta+t]);
        second_sum += T[i][theta+t];
    }
    return sqrtf((N*first_sum)-(second_sum*second_sum));
}

void get_relations(float T[M][N][l*KEY_SIZE], char m[M][KEY_SIZE], Relation* relations[KEY_SIZE]){
    int i, j, k;
    pthread_t thread_id;
    float sum_array[KEY_SIZE];
    float standard_deviation_array[KEY_SIZE];
    
    for(i=0; i<KEY_SIZE; i++){
        relations[i]=NULL;
    }
    for(i=0; i<M; i++){
        while(threads_number >= max_threads){
            sleep(500);
        }
        pthread_mutex_lock(&mutex_threads_number); 
        threads_number++;
        pthread_mutex_unlock(&mutex_threads_number); 
        Thread_args* args = malloc(sizeof(Thread_args));
        args->T = T[i];
        args->m = m[i];
        args->relations = relations;
        pthread_create(&thread_id, NULL, find_collisions, (void* ) args);
    }
    while(threads_number > 0){
        sleep(500);
    }
}

//check collisions for a given power trace 
void* find_collisions(void* args){
    Thread_args* args_casted = (Thread_args*) args;
    float** T = args_casted->T;
    char* m = args_casted->m;
    Relation** relations = args_casted->relations;
    float sum_array[KEY_SIZE*l];
    float std_dev_array[KEY_SIZE*l];
    compute_arrays(T, sum_array, std_dev_array);
    for(int j=0; j<KEY_SIZE; j++){//TODO capire se è giusto, magari il calcolo degli istanti di tempo è più difficile
        for(int k=j+1; k<KEY_SIZE; k++){//TODO in caso modificare anche qua
            if(collision(T, j*l, k*l, sum_array, std_dev_array)){
                /* if a collision is detected, two new relations are created:
                 * from byte j to k and viceversa. This to achieve better 
                 * performance when searching for a relation involving a 
                 * specific byte
                 */

                //controllare che la relazione non esissta già
                //usare mutex!
                pthread_mutex_lock(&mutex_relations); 
                
                Relation* new_relation = malloc(sizeof(Relation));
                char new_value = (m[j])^(m[k]);
                new_relation->in_relation_with = k;
                new_relation->value = new_value;
                new_relation->next = relations[j];
                relations[j] = new_relation;

                new_relation = malloc(sizeof(Relation));
                new_relation->in_relation_with = j;
                new_relation->value = new_value;
                new_relation->next = relations[k];
                relations[k] = new_relation;
                
                pthread_mutex_unlock(&mutex_relations); 
            }
        }      
    }
    pthread_mutex_lock(&mutex_threads_number); 
    threads_number--;
    pthread_mutex_unlock(&mutex_threads_number);
    pthread_exit(NULL);
}



/*
 Recursive function no, too much overhead
 */
void guess_key(Relation** relations){
    int i=0;
    int guessed[KEY_SIZE];
    for(i=0; i<KEY_SIZE; i++){
        guessed[i]=FALSE;
    }
    Subkey_element* subkeys_list = NULL;
    Subkey_element* new_subkey;
    int initial_key[KEY_SIZE_INT];
    
    for(i=0; i<KEY_SIZE; i++){
        if(guessed[i]==FALSE){
            new_subkey = guess_subkey(i, relations, guessed);
            new_subkey->next = subkeys_list;
            subkeys_list = new_subkey;
        }
    }
    
    for(i=0; i<KEY_SIZE_INT; i++){
        initial_key[i] = 0;
    }
    combine_subkeys(subkeys_list, initial_key);
    
    //free memory allocated for subkeys
    Subkey_element* subkey_pointer_current = subkeys_list;
    Subkey_element* subkey_pointer_next = subkeys_list->next;
    while(subkey_pointer_next!=NULL){
        free(subkey_pointer_current);
        subkey_pointer_current = subkey_pointer_next;
        subkey_pointer_next = subkey_pointer_current->next;
    }
    free(subkey_pointer_current);
}

Subkey_element* guess_subkey(int to_guess, Relation** relations, int* guessed){
    int i=0;
    unsigned char c=0;
    char xor_array[KEY_SIZE];
    char new_guessed[KEY_SIZE];
    for(i=0; i<KEY_SIZE; i++){
        new_guessed[i]=FALSE;
        xor_array[i] = 0x00;
    }
    new_guessed[to_guess] = TRUE;
    char random_value[KEY_SIZE];
    Subkey_element* new_subkeys = malloc(sizeof(Subkey_element));
    
    if(relations[to_guess]!=NULL){
        resolve_relations(to_guess, relations, new_guessed, xor_array);
    }
    for(c=0; c<256; c++){   //sostituire 256 con byte_space?
        for(i=0; i<KEY_SIZE; i++){
            random_value[i]=c;
        }
        for(i=0; i<KEY_SIZE_INT; i++){
            ((int*) ((new_subkeys->subkeys)[c]))[i] = (((int*) random_value)[i] ^ ((int*) xor_array)[i]) & ((int*) new_guessed)[i];     //int optimization
        }
    }
    for(i=0; i<KEY_SIZE; i++){
        guessed[i] = guessed[i] | new_guessed[i];   //controllare se funziona, è un or tra int e char
    }
    return new_subkeys;
}

void resolve_relations(int start, Relation** relations, char* new_guessed, char* xor_array){        
    new_guessed[start]=TRUE;
    Relation* pointer = relations[start];
    while(pointer!=NULL){
        if(new_guessed[pointer->in_relation_with]==FALSE){
            xor_array[pointer->in_relation_with] = xor_array[start] ^ (pointer->value);
            resolve_relations(pointer->in_relation_with, relations, new_guessed, xor_array);
        }            
        pointer = pointer->next;
    }
}


void combine_subkeys(Subkey_element* subkeys_list, int* partial_key){
    Subkey_element* list_pointer = subkeys_list;
    int** subkey_pointer = (int**) list_pointer->subkeys;
    char key[KEY_SIZE]; 
    int i=0, j=0;
    
    if(list_pointer->next == NULL){
        for(i=0; i<BYTE_SPACE; i++){
            for(j=0; j<KEY_SIZE_INT; j++){
                ((int*) key)[j] = partial_key[j] ^ subkey_pointer[i][j];
            }
            printf("%s\n", key);
        }
    }
    else{
        for(i=0; i<BYTE_SPACE; i++){
            for(j=0; j<KEY_SIZE_INT; j++){
                ((int*) key)[j] = partial_key[j] ^ subkey_pointer[i][j];
            }
            combine_subkeys(subkeys_list->next, (int*) key);
        }
    }
}