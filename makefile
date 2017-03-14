all:
	gcc server.c -g3 -o server.out
	gcc client.c -g3 -o client.out

clean:
	rm server.out
	rm client.out
