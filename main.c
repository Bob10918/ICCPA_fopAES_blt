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
#include "math.h"

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

int l;        //number of points acquired per instruction processing
float*** T;      //array containing all the power traces (divided per messages)
int N;        //number of power traces captured from a device processing N encryptions of the same message
int M;        //number of plaintext messages
float threshold;    //threshold for collision determination

void get_relations(float*** T, int N, char** m, int M, Relation** relations);
void guess_key(Relation** relations);
float standard_deviation(float** T, int theta, int t);
float covariance(float** T, int theta0, int theta1, int t);
void pearson(float** T, int theta0, int theta1, float* correlation);
Subkey_element* guess_subkey(int to_guess, Relation** relations, int* guessed);
void resolve_relations(int start, Relation** relations, char* new_guessed, char* xor_array);
void combine_subkeys(Subkey_element* subkeys_list, int* partial_key);

int main(int argc, char** argv) {
    /*
    //da capire bene come vengono passati i parametri
    l = argv[1]; 
    //power traces are provided as array of integers (each int is the measured power in the correspondant instant)
    T = argv[2];    
    N = sizeof(T[0])/sizeof(T[0][0]);     //controllare se funziona
    char** m = argv[3];     //array containing all the plaintext messages
    M = sizeof(m)/sizeof(m[0]);
    //TODO controllare se l divide N?
    threshold = argv[4];
    */
    char** m;
    
    Relation* relations[KEY_SIZE];
    
    get_relations(T, N, m, M, relations);
    guess_key(relations);
    
    //liberare memoria alla fine
    return (EXIT_SUCCESS);
}

//decision function returning true or false depending on whether the same data was involved in two given instructions theta0 and theta1
//compare the value of a synthetic criterion with a practically determined threshold
//as criterion we used the maximum Pearson correlation factor
//theta0 and theta1 are time instant at which instruction starts (for performance reasons)
int collision(float** T, int theta0, int theta1){
    float correlation[l];
    pearson(T, theta0, theta1, correlation);
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

void pearson(float** T, int theta0, int theta1, float* correlation){
    int i;
    for(i=0; i<l; i++){
        correlation[i] = covariance(T, theta0, theta1, i)/(standard_deviation(T, theta0, i)*standard_deviation(T, theta1, i));
    }
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
    return sqrtf((N*first_sum)-(second_sum*second_sum));
}

void get_relations(float*** T, int N, char** m, int M, Relation** relations){
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
                    
                    //controllare che la relazione non esissta già
                    Relation* new_relation = malloc(sizeof(Relation));
                    char new_value = (m[i][j])^(m[i][k]);
                    new_relation->in_relation_with = k;
                    new_relation->value = new_value;
                    new_relation->next = relations[j];
                    relations[j] = new_relation;
                    
                    new_relation = malloc(sizeof(Relation));
                    new_relation->in_relation_with = j;
                    new_relation->value = new_value;
                    new_relation->next = relations[k];
                    relations[k] = new_relation;
                }
            }      
        }
    }
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
    //liberare memoria subkeys
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
    for(c=0; c<256; c++){
        for(i=0; i<KEY_SIZE; i++){
            random_value[i]=c;
        }
        for(i=0; i<KEY_SIZE_INT; i++){
            ((int**) (new_subkeys->subkeys))[c][i] = ((int*) random_value)[i] ^ ((int*) xor_array)[i] & ((int*) new_guessed)[i];     //int optimization
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