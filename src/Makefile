CC = g++


CFLAGS = -Wall -g  -c  -I. -I../include/ -DPROF_TEST


OS = $(shell uname -s)
ifeq ($(OS),Darwin)
        SHAREDLIB_LINK_OPTIONS=-dynamiclib -Wl,-install_name -Wl,
else
        SHAREDLIB_LINK_OPTIONS=-shared -Wl,-soname,
endif



TARGETS = libmysqlbinlogapi.a libmysqlbinlogapi.so

OBJ = access_method_factory.o basic_content_handler.o basic_transaction_parser.o binary_log.o binlog_driver.o binlog_event.o field_iterator.o file_driver.o protocol.o resultset_iterator.o row_of_fields.o tcp_driver.o utilities.o value.o

all: $(TARGETS)

%.o: %.cpp
	$(CC) $(CFLAGS)  $*.cpp
libmysqlbinlogapi.a: $(OBJ)
	$(AR) crsv  $@ $^
	mkdir -p ../lib
	cp -f libmysqlbinlogapi.a ../lib
libmysqlbinlogapi.so: $(OBJ)
	$(CC) $(SHAREDLIB_LINK_OPTIONS)$@ -o $@ $^

install:
	@echo "installing library(to be done)"
clean:
	rm -f *o *~ $(TARGETS)
