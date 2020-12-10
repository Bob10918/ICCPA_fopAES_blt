CC=gcc
CFLAGS=-Wall -pedantic
LDFLAGS=-pthread -lm
OBJ = iccpa_fopaes_blt.o calculate_collisions_float.o calculate_collisions_double.o

default: iccpa_fopaes_blt

iccpa_fopaes_blt: $(OBJ)
	$(CC) $(CFLAGS) -o iccpa_fopaes_blt $(OBJ) $(LDFLAGS)
	
iccpa_fopaes_blt.o: main.c iccpa.h
	$(CC) $(CFLAGS) -o iccpa_fopaes_blt.o -c main.c
	
calculate_collisions_float.o: calculate_collisions.c iccpa.h
	$(CC) $(CFLAGS) -o calculate_collisions_float.o -c -DDATA_TYPE=float calculate_collisions.c
	
calculate_collisions_double.o: calculate_collisions.c iccpa.h
	$(CC) $(CFLAGS) -o calculate_collisions_double.o -c -DDATA_TYPE=double calculate_collisions.c


clean: 
	$(RM) iccpa_fopaes_blt *.o *~
