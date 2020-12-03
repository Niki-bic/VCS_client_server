# @file Makefile
# VCS
# Projekt: client-server
#
# authors:
#
# Binder Patrik             <ic19b030@technikum-wien.at>
# Ferchenbauer Nikolaus     <ic19b013@technikum-wien.at>
# @date 2020/12/02
#
# @version 1.0.0
#
# --------------------------------------------------------------------------

# ------------------------------------------------------------- variables --
# tools and options:

CC     =  gcc
CFLAGS = -Wall -Werror -Wextra -Wstrict-prototypes -Wformat=2 -pedantic \
         -fno-common -ftrapv -O3 -g -c -std=gnu11 
TFLAGS = -lsimple_message_client_commandline_handling
RM     =  rm -f

# filenames:

EXEC   = simple_message_client simple_message_server
SRC    = simple_message_client.c simple_message_server.c
OBJ    = $(SRC:.c=.o)
# HEADER = client_server.h
DOC    = doc
DOXY   = doxygen
DFILE  = Doxyfile

# --------------------------------------------------------------- targets --

# .PHONY

.PHONY: all clean doc distclean cleanall

all: $(EXEC)

clean:
	$(RM) $(EXEC) $(OBJ)

doc:
	$(DOXY) $(DFILE)

docclean:
	$(RM) -r $(DOC)

cleanall:
	$(RM) -r $(EXEC) $(OBJ) $(DOC)

# rules:

simple_message_client: simple_message_client.o
	$(CC) -o $@  $^  $(TFLAGS)

simple_message_server: simple_message_server.o
	$(CC) -o $@  $^

%.o: %.c # $(HEADER)
	$(CC) $(CFLAGS)  $<

# --------------------------------------------------------------------------
