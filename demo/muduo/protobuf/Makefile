all:client server
client:protobuf_client.cpp ../include/muduo/proto/codec.cc request.pb.cc
	g++ -std=c++11 $^ -o $@ -I../include -L ../lib -lmuduo_net -lmuduo_base -lpthread -lprotobuf -lz
server:protobuf_server.cpp ../include/muduo/proto/codec.cc request.pb.cc
	g++ -std=c++11 $^ -o $@ -I../include -L ../lib -lmuduo_net -lmuduo_base -lpthread -lprotobuf -lz

.PHONY:clean
clean:
	rm -f dict_client dict_server