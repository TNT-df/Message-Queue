mq_queue: mq_queuetest.cpp 
	g++ -std=c++11 $^ -o $@ -lgtest -lprotobuf -lpthread -lsqlite3
mq_exchange: mq_exchangetest.cpp 
	g++ -std=c++11 $^ -o $@ -lgtest -lprotobuf -lpthread -lsqlite3
filetest:filetest.cpp 
	g++ -std=c++11 $^ -o $@

.PHONY:clean
clean:
	rm -f mq_queue