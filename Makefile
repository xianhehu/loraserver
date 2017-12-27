### Application-specific constants

APP_NAME := loraserver

### Environment constants 
RELEASE_VERSION ?= 1
LIB_PATH ?= lib
ARCH ?=
CROSS_COMPILE ?=

OBJDIR = obj
INCLUDES = $(./*.h)

### External constant definitions
# must get library build option to know if mpsse must be linked or not


### Constant symbols

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

CFLAGS := -O2 -Wall -Wextra -std=gnu99 -Iinc -I.
VFLAG := -D VERSION_STRING="\"$(RELEASE_VERSION)\""

### Constants for Lora concentrator HAL library
# List the library sub-modules that are used by the application

LIB_INC = $(LIB_PATH)/aes/aes.h
LIB_INC += $(LIB_PATH)/base64/base64.h
LIB_INC += $(LIB_PATH)/json/cJSON.h
LIB_INC += $(LIB_PATH)/queue/queue.h
LIB_INC += $(LIB_PATH)/hashmap/hashmap.h

$LIB_PATH_INC = $(LIB_PATH)/aes
$LIB_PATH_INC += $(LIB_PATH)/base64
$LIB_PATH_INC += $(LIB_PATH)/json
$LIB_PATH_INC += $(LIB_PATH)/queue
$LIB_PATH_INC += $(LIB_PATH)/hashmap

### Linking options

LIBS := -lrt -lpthread -lm -lmysqlclient

### General build targets

all: $(APP_NAME)

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(APP_NAME)

### Sub-modules compilation

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: lib/aes/%.c $(LIB_INC)
	$(CC) -c -g $(CFLAGS) -I$(LIB_PATH)/inc $< -o $@

$(OBJDIR)/%.o: lib/queue/%.c
	$(CC) -c -g $(CFLAGS) $< -o $@

$(OBJDIR)/%.o: lib/hashmap/%.c
	$(CC) -c -g $(CFLAGS) -I$(LIB_PATH)/inc $< -o $@

$(OBJDIR)/%.o: lib/base64/%.c
	$(CC) -c -g $(CFLAGS) -I$(LIB_PATH)/inc $< -o $@

$(OBJDIR)/%.o: lib/json/%.c
	$(CC) -c -g $(CFLAGS) -I$(LIB_PATH)/inc $< -o $@

$(OBJDIR)/%.o: lib/log/%.c
	$(CC) -c -g $(CFLAGS) -I$(LIB_PATH)/inc $< -o $@

$(OBJDIR)/%.o: src/%.c $(INCLUDES)|$(LIB_INC) 
	$(CC) -c -g $(CFLAGS) -I$(LIB_PATH)/json -I$(LIB_PATH)/queue -I$(LIB_PATH)/hashmap -I$(LIB_PATH)/aes -I$(LIB_PATH)/base64 -I$(LIB_PATH)/log $< -o $@


### Main program compilation and assembly

$(APP_NAME): $(OBJDIR)/main.o $(OBJDIR)/queue.o $(OBJDIR)/queue_internal.o $(OBJDIR)/base64.o $(OBJDIR)/hashmap.o $(OBJDIR)/aes.o  $(OBJDIR)/gu.o $(OBJDIR)/mp.o $(OBJDIR)/cloud.o $(OBJDIR)/LoRaMacCrypto.o $(OBJDIR)/cJSON.o $(OBJDIR)/common.o $(OBJDIR)/cmac.o $(OBJDIR)/log.o $(OBJDIR)/sql.o
#$(APP_NAME): $(OBJDIR)
	$(CC) $< $(OBJDIR)/queue.o $(OBJDIR)/queue_internal.o $(OBJDIR)/base64.o $(OBJDIR)/hashmap.o $(OBJDIR)/aes.o  $(OBJDIR)/gu.o $(OBJDIR)/mp.o $(OBJDIR)/cloud.o $(OBJDIR)/LoRaMacCrypto.o $(OBJDIR)/cJSON.o $(OBJDIR)/common.o $(OBJDIR)/cmac.o $(OBJDIR)/log.o $(OBJDIR)/sql.o -o $@ $(LIBS)

### EOF
