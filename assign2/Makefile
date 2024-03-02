
OBJS2_1 = dberror.o storage_mgr.o buffer_mgr_stat.o buffer_mgr.o test_assign2_1.o
OBJS2_2 = dberror.o storage_mgr.o buffer_mgr_stat.o buffer_mgr.o test_assign2_2.o
MYOBJS = dberror.o storage_mgr.o buffer_mgr_stat.o buffer_mgr.o my_test1.o
CFLAGS = -Wall -g
CC = gcc
INCLUDES =
LIBS = 
HEADERS = dberror.h storage_mgr.h test_helper.h buffer_mgr_stat.h buffer_mgr.h dt.h

test_assign2_1:${OBJS2_1}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${OBJS2_1} ${LIBS}

test_assign2_2:${OBJS2_2}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${OBJS2_2} ${LIBS}	

my_test:${MYOBJS}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${MYOBJS} ${LIBS}

clean:
	-rm -f *.o test_assign2_1 test_assign2_2 my_test

.c.o: ${HEADERS}
	${CC} ${CFLAGS} ${INCLUDES} -c $<