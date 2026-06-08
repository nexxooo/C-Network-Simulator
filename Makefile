CC       = gcc
CFLAGS   = -Wall -Wextra -g -O2 -std=c23
CPPFLAGS = -Iinclude 

SRC_DIR  = src
OBJ_DIR  = obj
BIN_DIR  = bin
TEST_DIR = test

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Objects for compiling tests (excluding main.o)
LIB_OBJS = $(filter-out $(OBJ_DIR)/main.o, $(OBJS))

# Test sources and executables
TEST_SRCS  = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS  = $(patsubst $(TEST_DIR)/%.c, $(OBJ_DIR)/%.o, $(TEST_SRCS))
TEST_EXECS = $(patsubst $(TEST_DIR)/%.c, $(BIN_DIR)/%, $(TEST_SRCS))

EXEC     = $(BIN_DIR)/mon_programme

.PHONY: all clean fclean re run test

all: $(EXEC)

$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(BIN_DIR)

re: fclean all

run: all
	./$(EXEC)

test: $(TEST_EXECS)
	@for test_exec in $(TEST_EXECS); do \
		echo "Running $$test_exec..."; \
		$$test_exec; \
	done

