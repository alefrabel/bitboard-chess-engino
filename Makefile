CC = gcc
COMMON_FLAGS = -mpopcnt -DUSE_POPCNT -mbmi -DUSE_BMI -march=native
RELEASE_FLAGS = $(COMMON_FLAGS) -O3
DEBUG_FLAGS = $(COMMON_FLAGS) -g

SRC = funnyEngine.c
BASE_TARGET = funnyEngine

ifdef WINDOWS_BUILD
    TARGET = $(BASE_TARGET).exe
else
    TARGET = $(BASE_TARGET)
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
