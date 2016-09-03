CFLAGS = -c -Wall
CFLAGS+= -g
LDFLAGS = 
SOURCES= memalloc.c memalloc_test.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=memalloc_test
all: $(SOURCES) $(EXECUTABLE)
$(EXECUTABLE): $(OBJECTS)
	gcc $(OBJECTS) $(LDFLAGS) -o $@
.c.o:
	gcc $(CFLAGS) $< -o $@
clean:
	rm -rf *.o *~ memalloc
