/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#define DATA_TYPE float

#include "iccpa.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define TRUE 0xff
#define FALSE 0
#define BYTE_SPACE 256

#define KEY_SIZE 16     //key size in byte


int max_threads;    //max number of threads to run simultaneously
pthread_mutex_t mutex_relations = PTHREAD_MUTEX_INITIALIZER;

//struct for directories management
struct stat st = {0};


typedef struct {
    DATA_TYPE** T;
    char* m;
    Relation** relations;
}Thread_args;


void read_data(FILE* input, FILE* traces_pointers[plaintextlen][BYTE_SPACE], char plaintexts[M][plaintextlen]);
void get_relations(FILE* traces_pointers[plaintextlen][BYTE_SPACE], char m[M][KEY_SIZE], Relation** relations);
void* find_collisions(void* args);
DATA_TYPE standard_deviation(DATA_TYPE** T, int theta, int t);
DATA_TYPE covariance(DATA_TYPE** T, int theta0, int theta1, int t, DATA_TYPE* sum_array);
DATA_TYPE optimized_pearson(DATA_TYPE** T, int theta0, int theta1, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array);




void calculate_collisions_float(FILE* infile, int max_threads_number, Relation** relations){
    max_threads = max_threads_number;
    
    FILE* traces_pointers[plaintextlen][BYTE_SPACE];
    char m[M][plaintextlen];
    
    
    char fcurr_name[50];
    char to_concat[10];    
//    //if (stat("./iccpa_fopaes_temp", &st) == -1) {
//    mkdir("./iccpa_fopaes_temp", 0700);
//    //}
//    for(int i=0 ; i<plaintextlen ; i++){
//        for(int j=0 ; j<BYTE_SPACE ; j++){
//            strcpy(fcurr_name, "./iccpa_fopaes_temp/");
//            sprintf(to_concat, "%d", i+1);
//            strcat(fcurr_name, to_concat);
//            strcat(fcurr_name, "_");
//            sprintf(to_concat, "%d", j);
//            strcat(fcurr_name, to_concat);
//            traces_pointers[i][j] = fopen(fcurr_name, "w+");
//        }
//    }
//    
//    read_data(infile, traces_pointers, m);
//    
//    for(int i=0 ; i<plaintextlen ; i++){
//        for(int j=0 ; j<BYTE_SPACE ; j++){
//            fclose(traces_pointers[i][j]);
//        }
//    }
//    fclose(infile);
    
    for(int i=0 ; i<plaintextlen ; i++){
        for(int j=0 ; j<BYTE_SPACE ; j++){
            strcpy(fcurr_name, "./iccpa_fopaes_temp/");
            sprintf(to_concat, "%d", i+1);
            strcat(fcurr_name, to_concat);
            strcat(fcurr_name, "_");
            sprintf(to_concat, "%d", j);
            strcat(fcurr_name, to_concat);
            traces_pointers[i][j] = fopen(fcurr_name, "r");
        }
    }
    
    
    get_relations(traces_pointers, m, relations);
    
}

void read_traces_DATA_TYPE(FILE* input, FILE* traces_pointers[plaintextlen][BYTE_SPACE], char plaintexts[M][plaintextlen]){
    DATA_TYPE temp[plaintextlen][l];
    
    for(int j=0; j<N; j++){
        for(int i=0; i<plaintextlen; i++){
            fread( temp[i], sizeof(DATA_TYPE), l, input);
        }
        fseek(input, (nsamples-(l*plaintextlen))*sizeof(DATA_TYPE) , SEEK_CUR);
        fread( plaintexts[j], sizeof(char), plaintextlen, input);
        
        for(int i=0; i<plaintextlen; i++){
            uint8_t byte_value = (uint8_t) plaintexts[j][i];
            fwrite(temp[i], sizeof(DATA_TYPE), l, traces_pointers[i][byte_value]);
        }
    }
}

//decision function returning true or false depending on whether the same data was involved in two given instructions theta0 and theta1
//compare the value of a synthetic criterion with a practically determined threshold
//as criterion we used the maximum Pearson correlation factor
//theta0 and theta1 are time instant at which instruction starts (for performance reasons)
int collision(DATA_TYPE** T, int theta0, int theta1, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array){
    
    DATA_TYPE max_correlation = optimized_pearson(T, theta0, theta1, sum_array, std_dev_array);
    
    //check if max is greater than threshold
    if(max_correlation>threshold){
        return TRUE;
    }
    else{
        return FALSE;
    }
}

//efficiently compute in one step both sum and std deviation
void compute_arrays(DATA_TYPE** T, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array){
    DATA_TYPE sum;
    DATA_TYPE squared_sum;
    for(int i=0; i<(l*KEY_SIZE); i++){
        sum = 0;
        squared_sum = 0;
        for(int j=0; j<n; j++){
            sum += T[j][i];
            squared_sum += T[j][i]*T[j][i];
        }
        sum_array[i] = sum;
        std_dev_array[i] = sqrtf((N*squared_sum)-(sum*sum));
    }
}

DATA_TYPE optimized_pearson(DATA_TYPE** T, int theta0, int theta1, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array){
    int i;
    DATA_TYPE correlation;
    DATA_TYPE max_correlation = 0;
    for(i=0; i<l; i++){
        correlation = covariance(T, theta0, theta1, i, sum_array)  /  (std_dev_array[theta0+i] * std_dev_array[theta1+i]);
        if(correlation > max_correlation){
            max_correlation = correlation;
        }
    }
    return max_correlation;
}

//for optimized pearson
DATA_TYPE covariance(DATA_TYPE** T, int theta0, int theta1, int t, DATA_TYPE* sum_array){
    int i;
    DATA_TYPE first_sum=0;
    for(i=0; i<n; i++){
        first_sum += (T[i][theta0+t]*T[i][theta1+t]);
    }
    return (n*first_sum)-(sum_array[theta0+t]*sum_array[theta1+t]);
}

DATA_TYPE standard_deviation(DATA_TYPE** T, int theta, int t){
    int i;
    DATA_TYPE first_sum=0, second_sum=0;
    for(i=0; i<n; i++){
        first_sum += (T[i][theta+t])*(T[i][theta+t]);
        second_sum += T[i][theta+t];
    }
    return sqrtf((n*first_sum)-(second_sum*second_sum));
}

void get_relations(FILE* traces_pointers[plaintextlen][BYTE_SPACE], char m[M][KEY_SIZE], Relation* relations[KEY_SIZE]){
    DATA_TYPE** T = malloc(sizeof(DATA_TYPE*)*n);
    DATA_TYPE temp[l];
    int i=0, threads_count=0;
    
    pthread_t running_threads[max_threads];
    void* ul[1];
    ul[0] = malloc(sizeof(int));
    
    for(i=0; i<KEY_SIZE; i++){
        relations[i]=NULL;
    }
    while(i<M){
        while(i<M && threads_count<max_threads){
            i++;
            threads_count++;
            Thread_args* args = malloc(sizeof(Thread_args));

            for(int j=0; j<n; j++){
                T[j] = malloc(sizeof(DATA_TYPE)*l*KEY_SIZE);
                for(int k=0; k<plaintextlen; k++){
                    uint8_t byte_value = (uint8_t) m[i][k];
                    fread(temp, sizeof(DATA_TYPE), l, traces_pointers[k][byte_value]);
                    for(int q=0; q<l; q++){
                        T[j][k*l+q] = temp[q];
                    }
                }
            }
            for(int j=0; j<plaintextlen; j++){
                    uint8_t byte_value = (uint8_t) m[i][j];
                    rewind(traces_pointers[j][byte_value]);
            }

            args->T = T;
            args->m = m[i];
            args->relations = relations;
            pthread_create(&(running_threads[threads_count]), NULL, find_collisions, (void* ) args);
        }
        for(int j=0; j<threads_count; j++){
            pthread_join(running_threads[j], ul);
        }
        threads_count = 0;
    }
}

//check collisions for a given power trace 
void* find_collisions(void* args){
    Thread_args* args_casted = (Thread_args*) args;
    DATA_TYPE** T = args_casted->T;
    char* m = args_casted->m;
    Relation** relations = args_casted->relations;
    DATA_TYPE sum_array[KEY_SIZE*l];
    DATA_TYPE std_dev_array[KEY_SIZE*l];
    compute_arrays(T, sum_array, std_dev_array);
    for(int j=0; j<KEY_SIZE; j++){
        for(int k=j+1; k<KEY_SIZE; k++){
            if(collision(T, j*l, k*l, sum_array, std_dev_array)){
                /* if a collision is detected, two new relations are created:
                 * from byte j to k and viceversa. This to achieve better 
                 * performance when searching for a relation involving a 
                 * specific byte
                 */

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
    pthread_exit(NULL);
}