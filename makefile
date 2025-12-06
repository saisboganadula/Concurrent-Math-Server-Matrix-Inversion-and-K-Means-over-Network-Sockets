# Compiler and flags
CC = gcc
CFLAGS = -w -O2

# Directories
OBJ_DIR = mathserver/objects

# Source files
SERVER_SRC = mathserver/src/server.c
CLIENT_SRC = Client/client.c

# Object files
SERVER_OBJ = $(OBJ_DIR)/server.o
CLIENT_OBJ = $(OBJ_DIR)/client.o

# Executables (renamed to myclient)
SERVER = server
MYCLIENT = client  # Rename "client" to "myclient"

# Targets
all: $(SERVER) $(MYCLIENT)

$(SERVER): $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

$(MYCLIENT): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(SERVER_OBJ): $(SERVER_SRC)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(CLIENT_OBJ): $(CLIENT_SRC)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm -f $(SERVER) $(MYCLIENT) $(SERVER_OBJ) $(CLIENT_OBJ)

