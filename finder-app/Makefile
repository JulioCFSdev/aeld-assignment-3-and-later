CC := $(CROSS_COMPILE)gcc

CFLAGS := -Wall -Werror

TARGET := writer
SRC := writer.c
OBJ := writer.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

file-result: $(OBJ)
	file $(TARGET) $(OBJ) > ../assignments/assignment2/fileresult.txt

clean:
	rm -f $(TARGET) *.o
