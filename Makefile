CC = gcc

all: master

CFLAGS = -g -std=c89 -pedantic

INCLUDES = src/**/*.h

COMMON_DEPS = $(INCLUDES) Makefile

exe: master ${COMMON_DEPS}
	${CC} build/*.o -o run.out

master: src/master.c node user master_book utils pid_list load_constants ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

node: src/node/node.c utils master_book pid_list ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

pid_list: src/pid_list/pid_list.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

user: src/user/user.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

load_constants: src/load_constants/load_constants.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

master_book: src/master_book/master_book.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

utils: src/utils/utils.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

clean:
	rm -f build/* bin/*