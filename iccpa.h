/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   iccpa.h
 * Author: bob
 *
 * Created on 24 ottobre 2020, 12.21
 */

#ifndef ICCPA_H
#define ICCPA_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int N;        //number of power traces captured from a device processing N encryptions of the same message
int n;     //number of encryption of the same message
int nsamples;   //total number of samples per power trace
int l;        //number of samples per instruction processing (clock)
char sampletype;    //type of samples, f=float  d=double
int plaintextlen;   //length of plaintext in bytes
int M;        //number of plaintext messages
float threshold;    //threshold for collision determination


typedef struct Relation_s{
    int in_relation_with;
    char value;
    struct Relation_s* next;
}Relation;  //controllare se struct sono puntatori



void calculate_collisions_float(FILE* infile, int max_threads_number, Relation** relations);
void calculate_collisions_double(FILE* infile, int max_threads_number, Relation** relations);
    
    

#ifdef __cplusplus
}
#endif

#endif /* ICCPA_H */

