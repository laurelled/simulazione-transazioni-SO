CC = gcc

all: node ledger utils transaction

CFLAGS = -g -Wall -Wextra -Wpedantic -Wconversion -std=c89 -pedantic

INCLUDES = src/**/*.h

COMMON_DEPS = $(INCLUDES) Makefile

node: src/node/node.c utils ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $<

ledger: src/ledger/ledger.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $<

user: src/user/user.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $<

transaction: src/transaction.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $<

utils: src/utils/utils.c ${COMMON_DEPS}
	${CC} ${CFLAGS} -c $<

clean:
	rm -f build/* bin/*