CFLAGS  = -g -Wall -pedantic -I../inc
LFLAGS  =
CC      = g++
RM      = /bin/rm -rf
AR      = ar rc
RANLIB  = ranlib

LIBRARY = logutility.a

LIB_SRC = logutility.cpp

LIB_OBJ = $(patsubst %.cpp,%.o,$(LIB_SRC))

# pattern rule for object files
%.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

all: $(LIBRARY)

$(LIBRARY): $(LIB_OBJ)
	$(AR) $(LIBRARY) $(LIB_OBJ)
	$(RANLIB) $(LIBRARY) 

clean:
	$(RM) $(LIBRARY) $(LIB_OBJ) 
