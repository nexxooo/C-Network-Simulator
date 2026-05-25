CC       = gcc
CFLAGS   = -Wall -Wextra -g -O2 -std=c23
CPPFLAGS = -Iinclude 

SRC_DIR  = src
OBJ_DIR  = obj
BIN_DIR  = bin
TEST_DIR = test

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

EXEC     = $(BIN_DIR)/mon_programme

.PHONY: all clean fclean re run

all: $(EXEC)

$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(BIN_DIR)

re: fclean all

run: all
	./$(EXEC)
