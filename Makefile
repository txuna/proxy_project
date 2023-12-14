
LDLIBS = -lconfig

all: proxy

proxy: main.o sds.o tgx_net.o tgx_worker.o tgx_epoll.o tgx_util.o
	gcc -o proxy -g main.o sds.o tgx_net.o tgx_worker.o tgx_epoll.o tgx_util.o $(LDLIBS)

main.o: main.h main.c 
	gcc -c -o main.o main.c 

sds.o: sds.h sds.c
	gcc -c -o sds.o sds.c

tgx_net.o : tgx_net.h tgx_net.c 
	gcc -c -o tgx_net.o tgx_net.c 

tgx_worker.o : tgx_worker.h tgx_worker.c
	gcc -c -o tgx_worker.o tgx_worker.c

tgx_epoll.o : tgx_epoll.h tgx_epoll.c 
	gcc -c -o tgx_epoll.o tgx_epoll.c

tgx_util.o : tgx_util.h tgx_util.c 
	gcc -c -o tgx_util.o tgx_util.c

clean:
	rm *.o proxy