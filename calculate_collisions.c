/* 
 * File:   calculate_collisions.h
 * Author: Emanuele Pisano
 *
 */

#include "iccpa.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tgmath.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define TRUE 0xff
#define FALSE 0
#define BYTE_SPACE 256      //dimension of a byte, i.e. 2^8 = 256

#define KEY_SIZE 16     //key size in byte


#define MAKE_FN_NAME(name, type) name ## _ ## type
#define THREAD_ARGS(type) MAKE_FN_NAME(Thread_args, type)
#define CALCULATE_COLLISIONS(type) MAKE_FN_NAME(calculate_collisions, type)
#define READ_DATA(type) MAKE_FN_NAME(read_data, type)
#define GET_RELATIONS(type) MAKE_FN_NAME(get_relations, type)
#define FIND_COLLISIONS(type) MAKE_FN_NAME(find_collisions, type)
#define STANDARD_DEVIATION(type) MAKE_FN_NAME(standard_deviation, type)
#define COVARIANCE(type) MAKE_FN_NAME(covariance, type)
#define OPTIMIZED_PEARSON(type) MAKE_FN_NAME(optimized_pearson, type)
#define COLLISION(type) MAKE_FN_NAME(collision, type)
#define COMPUTE_ARRAYS(type) MAKE_FN_NAME(compute_arrays, type)

//struct to pass to threads as argument
typedef struct {
    DATA_TYPE** T;
    char* m;
    Relation** relations;
}THREAD_ARGS(DATA_TYPE);


void READ_DATA(DATA_TYPE) (FILE* input, char plaintexts[M][plaintextlen]);
void GET_RELATIONS(DATA_TYPE) (char m[M][KEY_SIZE], Relation** relations);
void* FIND_COLLISIONS(DATA_TYPE) (void* args);
DATA_TYPE STANDARD_DEVIATION(DATA_TYPE) (DATA_TYPE** T, int theta, int t);
DATA_TYPE COVARIANCE(DATA_TYPE) (DATA_TYPE** T, int theta0, int theta1, int t, DATA_TYPE* sum_array);
DATA_TYPE OPTIMIZED_PEARSON(DATA_TYPE) (DATA_TYPE** T, int theta0, int theta1, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array);
int COLLISION(DATA_TYPE) (DATA_TYPE** T, int theta0, int theta1, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array);
void COMPUTE_ARRAYS(DATA_TYPE) (DATA_TYPE** T, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array);



/*
 * Read data from input file and create the needed temp files.
 * Then call the method to calculate relations over them.
 */
void CALCULATE_COLLISIONS(DATA_TYPE) (FILE* infile, Relation** relations){
    
    char m[M][plaintextlen];
    
    char fcurr_name[50];
    char to_concat[10];    

    FILE* curr;
    mkdir("./iccpa_fopaes_blt_temp", 0700);
    for(int i=0 ; i<plaintextlen ; i++){
        for(int j=0 ; j<BYTE_SPACE ; j++){
            strcpy(fcurr_name, "./iccpa_fopaes_blt_temp/");
            sprintf(to_concat, "%d", i+1);
            strcat(fcurr_name, to_concat);
            strcat(fcurr_name, "_");
            sprintf(to_concat, "%d", j);
            strcat(fcurr_name, to_concat);
            curr = fopen(fcurr_name, "w+");
            fclose(curr);
        }
    }
    
    READ_DATA(DATA_TYPE)(infile, m);
    
    fclose(infile);    
    
    GET_RELATIONS(DATA_TYPE)(m, relations);
    
}

/*
 * Correctly reads data from the file, basing on the data type
 */
void READ_DATA(DATA_TYPE)(FILE* input, char plaintexts[M][plaintextlen]){
    DATA_TYPE temp[plaintextlen][l];
    FILE* fcurr;
    char fcurr_name[50];
    char to_concat[10];
    
    for(int j=0; j<N; j++){
        //read the first l*plaintextlen samples (corresponding to first round computation)
        for(int i=0; i<plaintextlen; i++){
            fread( temp[i], sizeof(DATA_TYPE), l, input);
        }
        //skip the rest of the computation, useless for the attack
        fseek(input, (nsamples-(l*plaintextlen))*sizeof(DATA_TYPE) , SEEK_CUR);
        //read the plaintext
        fread( plaintexts[j], sizeof(char), plaintextlen, input);
        
        //store the data read in the relative temp files
        for(int i=0; i<plaintextlen; i++){
            uint8_t byte_value = (uint8_t) plaintexts[j][i];
            strcpy(fcurr_name, "./iccpa_fopaes_blt_temp/");
            sprintf(to_concat, "%d", i+1);
            strcat(fcurr_name, to_concat);
            strcat(fcurr_name, "_");
            sprintf(to_concat, "%d", byte_value);
            strcat(fcurr_name, to_concat);
            fcurr = fopen(fcurr_name, "a");
            fwrite(temp[i], sizeof(DATA_TYPE), l, fcurr);
            fclose(fcurr);            
        }
    }
}


/*
 * Decision function returning true or false depending on whether the same data was involved in two given instructions theta0 and theta1 
 * compare the value of a synthetic criterion with a practically determined threshold
 * as criterion we used the maximum Pearson correlation factor
 * theta0 and theta1 are time instant at which instruction starts (for performance reasons)
 */
int COLLISION(DATA_TYPE)(DATA_TYPE** T, int theta0, int theta1, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array){
    
    DATA_TYPE max_correlation = OPTIMIZED_PEARSON(DATA_TYPE)(T, theta0, theta1, sum_array, std_dev_array);
    
    //check if max is greater than threshold
    if(max_correlation>threshold){
        double one = 1.0000;
        if(max_correlation != one) printf("true: %f\n", max_correlation);
        return TRUE;
    }
    else{
        printf("false: %f\n", max_correlation);
        return FALSE;
    }
}

/*
 * Efficiently compute in one step both sum and standard deviation
 */
void COMPUTE_ARRAYS(DATA_TYPE)(DATA_TYPE** T, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array){
    DATA_TYPE sum;
    DATA_TYPE squared_sum;
    DATA_TYPE std_dev;
    for(int i=0; i<(l*KEY_SIZE); i++){
        sum = 0;
        squared_sum = 0;
        for(int j=0; j<n; j++){
            sum += T[j][i];
            squared_sum += T[j][i]*T[j][i];
        }
        sum_array[i] = sum;
        std_dev = (n*squared_sum)-(sum*sum);
        // check for unreal values caused by floating point precision
        if(std_dev < 0){
            std_dev_array[i] = 0;
        }
        else{
            std_dev_array[i] = sqrt(std_dev);
        }
    }
}

/*
 * Efficiently compute pearson correlation factor
 */
DATA_TYPE OPTIMIZED_PEARSON(DATA_TYPE)(DATA_TYPE** T, int theta0, int theta1, DATA_TYPE* sum_array, DATA_TYPE* std_dev_array){
    int i;
    DATA_TYPE correlation;
    DATA_TYPE max_correlation = 0;
    for(i=0; i<l; i++){
        // check for arithmetic overflow
        if(std_dev_array[theta0+i] != 0 && std_dev_array[theta1+i] != 0){
            correlation = COVARIANCE(DATA_TYPE)(T, theta0, theta1, i, sum_array)  /  ( std_dev_array[theta0+i] * std_dev_array[theta1+i] );
        }
        else{
            max_correlation = 1;
            break;
        }
        if(correlation > max_correlation){
            max_correlation = correlation;
        }
    }
    return max_correlation;
}

/*
 * Compute covariance of two given time sequences of samples
 */
DATA_TYPE COVARIANCE(DATA_TYPE)(DATA_TYPE** T, int theta0, int theta1, int t, DATA_TYPE* sum_array){
    int i;
    DATA_TYPE first_sum=0;
    for(i=0; i<n; i++){
        first_sum += (T[i][theta0+t]*T[i][theta1+t]);
    }
    return (n*first_sum)-(sum_array[theta0+t]*sum_array[theta1+t]);
}

/*
 * Compute standard deviation of a given time sequence of samples
 */
DATA_TYPE STANDARD_DEVIATION(DATA_TYPE)(DATA_TYPE** T, int theta, int t){
    int i;
    DATA_TYPE first_sum=0, second_sum=0;
    for(i=0; i<n; i++){
        first_sum += (T[i][theta+t])*(T[i][theta+t]);
        second_sum += T[i][theta+t];
    }
    return sqrt((n*first_sum)-(second_sum*second_sum));
}

/*
 * Infer relations abut the key starting from the samples, storing them all in an array of linked lists
 * Spawn a thread for every trace
 */
void GET_RELATIONS(DATA_TYPE)(char m[M][KEY_SIZE], Relation* relations[KEY_SIZE]){
    DATA_TYPE** T;
    DATA_TYPE temp[l];
    int i=0, threads_count=0;
    pthread_mutex_init(&mutex_relations, NULL);
    
    pthread_t running_threads[max_threads];
    void* ul[1];
    ul[0] = malloc(sizeof(int));
    
    FILE* fcurr;
    char fcurr_name[50];
    char to_concat[10];
    
    for(i=0; i<KEY_SIZE; i++){
        relations[i]=NULL;
    }
    while(i<M){
        while(i<M && threads_count<max_threads){
            T = malloc(sizeof(DATA_TYPE*)*n);
            THREAD_ARGS(DATA_TYPE) *args = malloc(sizeof(THREAD_ARGS(DATA_TYPE)));

            for(int j=0; j<n; j++){
                T[j] = malloc(sizeof(DATA_TYPE)*l*KEY_SIZE);
                for(int k=0; k<plaintextlen; k++){
                    uint8_t byte_value = (uint8_t) m[i][k];
                    strcpy(fcurr_name, "./iccpa_fopaes_blt_temp/");
                    sprintf(to_concat, "%d", k+1);
                    strcat(fcurr_name, to_concat);
                    strcat(fcurr_name, "_");
                    sprintf(to_concat, "%d", byte_value);
                    strcat(fcurr_name, to_concat);
                    fcurr = fopen(fcurr_name, "r");
                    fread(temp, sizeof(DATA_TYPE), l, fcurr);
                    fclose(fcurr);
                    for(int q=0; q<l; q++){
                        T[j][k*l+q] = temp[q];
                    }
                }
            }

            args->T = T;
            args->m = m[i];
            args->relations = relations;
            pthread_create(&(running_threads[threads_count]), NULL, FIND_COLLISIONS(DATA_TYPE), (void* ) args);
            
            i++;
            threads_count++;
        }
        for(int j=0; j<threads_count; j++){
            pthread_join(running_threads[j], ul);
        }
        threads_count = 0;
    }
}

/*
 * Check collisions for a given power trace 
 */
void* FIND_COLLISIONS(DATA_TYPE)(void* args){
    THREAD_ARGS(DATA_TYPE)* args_casted = (THREAD_ARGS(DATA_TYPE)*) args;
    DATA_TYPE** T = args_casted->T;
    char* m = args_casted->m;
    Relation** relations = args_casted->relations;
    DATA_TYPE sum_array[KEY_SIZE*l];
    DATA_TYPE std_dev_array[KEY_SIZE*l];
    
    COMPUTE_ARRAYS(DATA_TYPE)(T, sum_array, std_dev_array);
    for(int j=0; j<KEY_SIZE; j++){
        for(int k=j+1; k<KEY_SIZE; k++){
            if(COLLISION(DATA_TYPE)(T, j*l, k*l, sum_array, std_dev_array)){
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