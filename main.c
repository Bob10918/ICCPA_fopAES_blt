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

#include "iccpa.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define TRUE 0xff
#define FALSE 0
#define BYTE_SPACE 256

#define KEY_SIZE 16     //key size in byte
#define KEY_SIZE_INT KEY_SIZE/(sizeof(int)/sizeof(char))    //key size in integers




typedef struct Subkey_element_s{
    char subkeys[BYTE_SPACE][KEY_SIZE];
    struct Subkey_element_s* next;
}Subkey_element;


void guess_key(Relation** relations);
Subkey_element* guess_subkey(int to_guess, Relation** relations, int* guessed);
void resolve_relations(int start, Relation** relations, char* new_guessed, char* xor_array);
void combine_subkeys(Subkey_element* subkeys_list, int* partial_key);


int main(int argc, char** argv) {
    
    FILE* infile = fopen(argv[1], "r");
    fread(&N, sizeof(uint32_t), 1, infile);
    int N_print = N;      //only for debugging
    n = 30;     //could be set to a different value
    fread(&nsamples, sizeof(uint32_t), 1, infile);
    l = 15;      //could be set at nsamples/KEY_SIZE/number_of_rounds
    fread(&sampletype, sizeof(char), 1, infile);
    char st = sampletype;
    uint8_t plaintextlen_temp;
    fread(&plaintextlen_temp, sizeof(uint8_t), 1, infile);
    plaintextlen = (int) plaintextlen_temp;
    int ptl = plaintextlen;     //only for debugging
    
    M = N;      //could be changed
    threshold = 0.9;    //could be changed
    int max_threads = 100;
    
    Relation* relations[KEY_SIZE];
    
    switch (sampletype){
        case 'f':
            calculate_collisions_float(infile, max_threads, relations);
          break;

        case 'd':
            calculate_collisions_double(infile, max_threads, relations);
          break;

        default:
            exit(-1);
    }
    
    
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
    
    return (EXIT_SUCCESS);
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