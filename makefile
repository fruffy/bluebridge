all:
	gcc server.c -o server.out
	gcc client.c -o client.out

clean:
	rm server.out
	rm client.out
