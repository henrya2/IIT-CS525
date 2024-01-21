
OBJS =  dberror.o storage_mgr.o test_assign1_1.o
MYOBJS = dberror.o storage_mgr.o my_test1.o
CFLAGS = -Wall -g
CC = gcc
INCLUDES =
LIBS = 
HEADERS = dberror.h storage_mgr.h test_helper.h

test_assign1:${OBJS}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${OBJS} ${LIBS}

my_test:${MYOBJS}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${MYOBJS} ${LIBS}

my_clean:
	-rm -f *.o my_test
clean:
	-rm -f *.o test_assign1

.c.o: ${HEADERS}
	${CC} ${CFLAGS} ${INCLUDES} -c $<