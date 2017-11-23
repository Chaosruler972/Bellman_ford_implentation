main: main.c
	gcc -Wall -Wextra -pedantic -g main.c -o main -pthread
clear: 
	rm main
