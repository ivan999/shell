CFLAGS = -Wall -g 

shell: main.c cmdshell.o
	gcc $(CFLAGS) main.c lexer.o cmdshell.o -o shell

cmdshell.o: cmdshell.c cmdshell.h separators.h dynarr.h lexer.o
	gcc $(CFLAGS) cmdshell.c -c

lexer.o: lexer.c lexer.h separators.h dynarr.h
	gcc $(CFLAGS) lexer.c -c

clean:
	rm *.o shell
