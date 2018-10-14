#
# Projekt: IPK projekt 1 
# Autor:   Michal Melichar (xmelic17)
# Datum:   22.03.2014
# 

CC=gcc
CFLAGS=-std=c99 -Wall -pedantic -g

ftpclient: ftpclient.o
				 $(CC) $(CFLAGS) ftpclient.c -o ftpclient
