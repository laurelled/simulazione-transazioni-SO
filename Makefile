CC = gcc

all: exe

CFLAGS = -g -std=c89 -pedantic

INCLUDES = src/**/*.h

COMMON_DEPS = $(INCLUDES) Makefile

conf1: CFLAGS += -DCONF1
conf1: master ${COMMON_DEPS}
	${CC} build/*.o -o run.out

conf2: CFLAGS += -DCONF2
conf2: master ${COMMON_DEPS}
	${CC} -DCONF2 build/*.o -o run.out

conf3: CFLAGS += -DCONF3
conf3: master ${COMMON_DEPS}
	${CC} build/*.o -o run.out

exe: master ${COMMON_DEPS}
	${CC} build/*.o -o run.out

master: src/master/master.c node user master_book utils ipc_functions load_constants ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

node: src/node/node.c utils master_book ipc_functions ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o build/$@.o

ipc_functions: src/ipc_functions/ipc_functions.c ${COMMON_DEPS}
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