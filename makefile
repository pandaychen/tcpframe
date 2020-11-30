CC = gcc
CXX = gcc

#MYFLAGS = -MMD -g -Wall -pthread
MYFLAGS = -g -Wall -O2
INC_DIR = -I ./include
LIB = 

TAR_DIR = ./
SRC_DIR = ./src
OBJ_DIR = ./

TARGET = $(TAR_DIR)main

#OBJS = \
#       $(OBJ_DIR)main.o \
#	   $(OBJ_DIR)hash_fun.o \
#	   $(OBJ_DIR)shash.o \

EXTENSION=c
OBJS=$(patsubst $(SRC_DIR)/%.$(EXTENSION), $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.$(EXTENSION)))

OBJ_SUB_DIRS = $(sort $(dir $(OBJS)))

DEPENDS = $(patsubst %.o, %.d, $(OBJS))


.PHONY: all clean init 


#compile start
all: init $(OBJS) $(TARGET)
	@echo $(OBJS)

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.$(EXTENSION) 
	$(CC) $< -o $@ -c $(MYFLAGS)  $(INC_DIR) 

$(TARGET):$(TAR_DIR)main.c $(OBJS)
	@echo $(OBJS)
	$(CXX) -o $@ $^ $(LIB) $(MYFLAGS) $(INC_DIR)

$(OBJ_DIR)%.o: $(SRC_DIR)%.c
	@echo $(OBJS)
	#$(CC) $(MYFLAGS) -c  -o $@ $(INC_DIR) $<
	$(CC) $< -o $@ -c $(MYFLAGS) $(INC_DIR) 

clean:
	rm -f $(OBJS) $(TARGET) $(DEPENDS)

init:
	mkdir -p $(sort ./ $(TAR_DIR) $(SRC_DIR) $(OBJ_DIR) $(OBJ_SUB_DIRS))

-include $(DEPENDS)
