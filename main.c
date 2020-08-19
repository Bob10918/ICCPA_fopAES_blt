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

#define TRUE 1
#define FALSE 0
#define KEY_SIZE 16     //key size in byte

typedef struct r{
    int in_relation_with;
    char value;
    r* next;
}relation;

const int l;        //number of points acquired per instruction processing
const float*** T;      //array containing all the power traces (divided per messages)
const int N;        //number of power traces captured from a device processing N encryptions of the same message
const int M;        //number of plaintext messages
const float threshold;    //threshold for collision determination

int main(int argc, char** argv) {
    //da capire bene come vengono passati i parametri
    l = argv[1]; 
    //power traces are provided as array of integers (each int is the measured power in the correspondant instant)
    T = argv[2];    
    N = sizeof(T[0])/sizeof(T[0][0]);     //controllare se funziona
    char** m = argv[3];     //array containing all the plaintext messages
    M = sizeof(m)/sizeof(m[0]);
    //TODO controllare se l divide N?
    threshold = argv[4];
    struct relation relations[KEY_SIZE] = get_relations(T, N, m, M);
    
    
    
    return (EXIT_SUCCESS);
}

//decision function returning true or false depending on whether the same data was involved in two given instructions theta0 and theta1
//compare the value of a synthetic criterion with a practically determined threshold
//as criterion we used the maximum Pearson correlation factor
//theta0 and theta1 are time instant at which instruction starts (for performance reasons)
int collision(float** T, int theta0, int theta1){
    float correlation[l] = pearson(T, theta0, theta1);
    float max = correlation[0];
    for(int i=1; i<l; i++){
        if(correlation[i]>max){
            max = correlation[i];
        }
    }
    if(max>threshold){
        return TRUE;
    }
    else{
        return FALSE;
    }
    //TODO migliorabile anche questa, tipo calcolando il max direttamente in pearson, appena dopo averlo calcolato
}

float* pearson(float** T, int theta0, int theta1){
    float correlation[l];
    int i;
    for(i=0; i<l; i++){
        correlation[i] = covariance(T, theta0, theta1, i)/(standard_deviation(T, theta0, i)*standard_deviation(T, theta1, i));
    }
    return correlation;
}

float* optimized_pearson(){
    //TODO
    //per ottimizzare forse si potrebbe calcolare insieme covariance e standard_deviation (per accedere meglio agli array, da verificare con qualche prova)
}

float covariance(float** T, int theta0, int theta1, int t){
    int i;
    float first_sum=0, second_sum=0, third_sum=0;
    for(i=0; i<N; i++){
        first_sum += (T[i][theta0+t]*T[i][theta1+t]);
        second_sum += T[i][theta0+t];
        third_sum += T[i][theta1+t];
    }
    return (N*first_sum)-(second_sum*third_sum);
}

float standard_deviation(float** T, int theta, int t){
    int i;
    float first_sum=0, second_sum=0;
    for(i=0; i<N; i++){
        first_sum += (T[i][theta+t])*(T[i][theta+t]);
        second_sum += T[i][theta+t];
    }
    return sqrt((N*first_sum)-(second_sum*second_sum));
}

struct relation* get_relations(float*** T, int N, char** m, int M){
    struct relation relations[KEY_SIZE];
    int i, j, k;
    for(i=0; i<KEY_SIZE; i++){
        relations[i]=NULL;
    }
    for(i=0; i<M; i++){
        for(j=0; j<KEY_SIZE; j++){//TODO capire se è giusto, magari il calcolo degli istanti di tempo è più difficile
            for(k=j+1; k<KEY_SIZE; k++){//TODO in caso modificare anche qua
                if(collision(T[i], j*l, k*l)){
                    /* if a collision is detected, two new relations are created:
                     * from byte j to k and viceversa. This to achieve better 
                     * performance when searching for a relation involving a 
                     * specific byte
                     */
                    struct relation new_relation = malloc(sizeof(struct relation));
                    char new_value = (m[i][j])^(m[i][k]);
                    new_relation->in_relation_with = k;
                    new_relation->value = new_value;
                    new_relation->next = relations[j];
                    relations[j] = new_relation;
                    
                    new_relation = malloc(sizeof(struct relation));
                    new_relation->in_relation_with = j;
                    new_relation->value = new_value;
                    new_relation->next = relations[k];
                    relations[k] = new_relation;
                }
            }      
        }
    }
    return relations;
}