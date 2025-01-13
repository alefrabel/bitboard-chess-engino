CC = gcc
COMMON_FLAGS = -mpopcnt -DUSE_POPCNT -mbmi -DUSE_BMI -march=native
RELEASE_FLAGS = $(COMMON_FLAGS) -O3
DEBUG_FLAGS = $(COMMON_FLAGS) -g

SRC = funnyEngine.c
UNIX_TARGET = funnyEngine

ifeq ($(OS),Windows_NT)
    TARGET = $(UNIX_TARGET).exe
else
    TARGET = $(UNIX_TARGET)
endif

all: release

release: $(SRC)
	$(CC) $(RELEASE_FLAGS) $^ -o $(TARGET)
	@echo "compilation complete (release)"

debug: $(SRC)
	$(CC) $(DEBUG_FLAGS) $^ -o $(TARGET)
	@echo "compilation complete (debug)"

clean:
	rm -f $(TARGET)
	@echo "cleaned build files"

.PHONY: all release debug clean