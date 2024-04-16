
OBJS_1 = dberror.o storage_mgr.o buffer_mgr_stat.o buffer_mgr.o expr.o rm_serializer.o record_mgr.o btree_mgr.o btree_mgr_private.o test_assign4_1.o
OBJS_expr = dberror.o storage_mgr.o buffer_mgr_stat.o buffer_mgr.o expr.o rm_serializer.o record_mgr.o btree_mgr.o btree_mgr_private.o test_expr.o
CFLAGS = -Wall -g
CC = gcc
INCLUDES =
LIBS = 
HEADERS = dberror.h storage_mgr.h test_helper.h buffer_mgr_stat.h buffer_mgr.h dt.h expr.h tables.h record_mgr.h record_mgr_private.h btree_mgr.h btree_mgr_private.h

test_assign4_1:${OBJS_1}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${OBJS_1} ${LIBS}

test_expr:${OBJS_expr}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${OBJS_expr} ${LIBS}	

clean:
	-rm -f *.o test_assign4_1 test_expr

.c.o: ${HEADERS}
	${CC} ${CFLAGS} ${INCLUDES} -c $<