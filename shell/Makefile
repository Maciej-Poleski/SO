
OBJECTS=mshell.o commands.o cparse.o
HEADERS=commands.h cparse.h config.h

NAME=mshell

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CC) -o $(NAME) $(OBJECTS)

mshell.o: $(HEADERS)

commands.o: commands.h config.h
cparse.o: cparse.h config.h

clean:
	rm $(OBJECTS)  $(NAME)
