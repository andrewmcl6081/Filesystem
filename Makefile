msf: msf.c res/baseCommands.c
	gcc -g msf.c res/baseCommands.c -o msf

clean:
	rm ./msf 