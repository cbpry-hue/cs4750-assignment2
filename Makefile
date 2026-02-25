# Compiler and Flags
CC = gcc
CFLAGS = -Wall -g

# Executables
TARGET1 = oss
TARGET2 = worker

# Object Files
OBJ1 = oss.o
OBJ2 = worker.o

# Default target: builds both executables [cite: 144]
all: $(TARGET1) $(TARGET2)

# Link oss executable
$(TARGET1): $(OBJ1)
	$(CC) $(CFLAGS) -o $(TARGET1) $(OBJ1)

# Link worker executable
$(TARGET2): $(OBJ2)
	$(CC) $(CFLAGS) -o $(TARGET2) $(OBJ2)

# Compile oss.c to oss.o
oss.o: oss.c
	$(CC) $(CFLAGS) -c oss.c

# Compile worker.c to worker.o
worker.o: worker.c
	$(CC) $(CFLAGS) -c worker.c

# Clean up build files [cite: 192]
clean:
	rm -f *.o $(TARGET1) $(TARGET2)