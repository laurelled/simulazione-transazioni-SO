CC = gcc

CFLAGS = -g -std=c89 -pedantic

HEADERS = src/**/*.h
COMMON_DEPS = ${HEADERS} Makefile

run.out: build/*.o ${COMMON_DEPS}
	${CC} build/*.o -o $@

conf1: CFLAGS += -DCONF1
conf1: build/master.o build/node.o ${COMMON_DEPS}
	${CC} ${CFLAGS} -c src/node/node.c -o build/node.o
	${CC} ${CFLAGS} -c src/master.c -o build/master.o
	${CC} build/*.o -o run.out
conf2: CFLAGS += -DCONF2
conf2: build/master.o build/node.o ${COMMON_DEPS}
	${CC} ${CFLAGS} -c src/node/node.c -o build/node.o
	${CC} ${CFLAGS} -c src/master.c -o build/master.o
	${CC} build/*.o -o run.out

conf3: CFLAGS += -DCONF3
conf3: build/master.o build/node.o ${COMMON_DEPS}
	${CC} ${CFLAGS} -c src/node/node.c -o build/node.o
	${CC} ${CFLAGS} -c src/master.c -o build/master.o
	${CC} build/*.o -o run.out

build/master.o: src/master.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o $@

build/master_utils.o: src/master_utils/master_utils.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o $@

build/node.o: src/node/node.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o $@

build/ipc.o: src/ipc/ipc.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o $@

build/user.o: src/user/user.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o $@

build/load_constants.o: src/load_constants/load_constants.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o $@

build/master_book.o: src/master_book/master_book.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o $@

build/utils.o: src/utils/utils.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -f build/*

all: run.out
