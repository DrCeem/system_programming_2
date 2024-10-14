all : jobExecutorServer jobCommander progDelay

jobExecutorServer:
	gcc -o bin/jobExecutorServer src/jobExecutorServer.c src/queue.c -lpthread -Iinclude
jobCommander:
	gcc -o bin/jobCommander src/jobCommander.c
progDelay:
	gcc -o bin/progDelay src/progDelay.c 
clean:
	rm -f bin/jobExecutorServer bin/jobCommander bin/progDelay