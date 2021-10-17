# Roteamento  Bellman-Ford distribuído
# Alunos: Murilo Xavier de Oliveira e Rodrigo Vogt

# Makefile
all: prog

prog: *.c *.h
	gcc funcs.h readFiles.c router.c -D_REENTRANT -lpthread -o main -Wall
