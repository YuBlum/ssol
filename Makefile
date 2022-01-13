FLAGS=-g -Wall 
STD=-std=c99

ssol: ssol.c
	gcc $(FLAGS) $(STD) ssol.c -o ssol
