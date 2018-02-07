CC 		:= g++
LD		:= ld
AR		:= ar

TARGET 		= ffplayer

DIR_BASE	:= .
DIR_INC		:= ./src

DIR_SRC		:= ./src
DIR_OBJ		:= ./obj

PWD_DIR		:= $(shell pwd)
DIR_LIB		:= $(PWD_DIR)/bin

TARGET_BIN_PATH	:= $(DIR_LIB)
																						  
INCLUDES 	= -I${DIR_BASE}/ -I${DIR_INC}/ `pkg-config --cflags glib-2.0` -I../install/include 
OBJ_PATH	= ${DIR_OBJ}/

ver = debug
ifeq ($(ver), debug)
CFLAGS 		= -std=c++11 -Wall -g -fpermissive 
else 
CFLAGS 		= -std=c++11 -Wall -fpermissive 
endif

ARFLAGS 	= -rc

#-ldl
LDFLAGS 	= -L$(DIR_LIB) -L../install/lib -lavfilter -lswresample -lavcodec -lswscale -lavdevice -lavformat -lavutil -lSDL2 -lpthread -lm 

ALL_SRC		= $(wildcard ${DIR_SRC}/*.cpp)
ALL_OBJ		= $(patsubst %.cpp, %.o, $(notdir ${ALL_SRC}))

ALL_OBJ_POS	:= $(addprefix $(OBJ_PATH), $(ALL_OBJ))


all: $(TARGET)
$(TARGET): $(ALL_OBJ)
	@if test ! -d $(TARGET_BIN_PATH); \
	then \
		mkdir $(TARGET_BIN_PATH); \
	fi
	$(CC) $(CFLAGS) -o $(TARGET_BIN_PATH)/$@ $(ALL_OBJ_POS) $(LDFLAGS)

%.o: ${DIR_SRC}/%.cpp
	@if test ! -d $(DIR_OBJ); \
	then \
		mkdir $(DIR_OBJ); \
	fi
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o ${DIR_OBJ}/$@

clean:
	rm -rf $(DIR_OBJ) $(TARGET_BIN_PATH)/core $(TARGET_BIN_PATH)/$(TARGET)
