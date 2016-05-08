all:
	g++ lt.cc -std=c++11 -lpthread -lcurl -o lt

clean:
	rm lt
