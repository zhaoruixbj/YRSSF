all:../build/YRSSF
	
.dep:\
    classes.hpp.gch       \
    client.hpp.gch        \
    crypt.hpp.gch         \
    daemon.hpp.gch        \
    global.hpp.gch        \
    func.hpp.gch          \
    live.hpp.gch          \
    nint.hpp.gch          \
    server-base.hpp.gch   \
    server.hpp.gch        \
    webserver.hpp.gch     \
    ysdb.hpp.gch          \
    ysconnection.hpp.gch  \
    key.hpp.gch           \
    rwmutex.hpp.gch       \
    base64.hpp.gch        \
    scriptqueue.hpp.gch   \
    scriptworker.hpp.gch  \
    languagesolver.hpp.gch\
    cache.hpp.gch         \
    threadpool.hpp.gch    \
    main.cpp              \
    sqlite/sqlite3.o      \
    ./zlib/libz.a         \
    ./aes/aes.o           \
    ./lua/src/liblua.a    \
    ./leveldb/out-static/libleveldb.a \
    lua-cjson/lua_cjson.o             \
    ./lwan/build/common/liblwan.a
	
../build/YRSSF : .dep
	g++ main.cpp                       \
	 -pthread                          \
	 ./leveldb/out-static/libleveldb.a \
	 ./leveldb/out-static/libmemenv.a  \
	 ./lua/src/liblua.a                \
	 ./lwan/build/common/liblwan.a     \
	 ./zlib/libz.a                     \
	 ./aes/aes.o                       \
	 ./lua-cjson/lua_cjson.o           \
	 ./lua-cjson/strbuf.o              \
	 ./lua-cjson/fpconv.o              \
	 ./sqlite/sqlite3.o                \
	 -ldl                              \
	 -DWEB1                            \
	 -I ./lua/src                      \
	 -I ./lwan/common                  \
	 -I ./leveldb/include              \
	 -I ./zlib                         \
	 -I ./aes                          \
	 -I ./                             \
	 -std=c++0x                        \
	 -ljpeg                            \
	 -lssl                             \
	 -lcrypto                          \
	 -o ../build/YRSSF
android : .dep
	g++ -static main.cpp               \
	 -pthread                          \
	 ./leveldb/out-static/libleveldb.a \
	 ./leveldb/out-static/libmemenv.a  \
	 ./lwan/build/common/liblwan.a     \
	 ./lua/src/liblua.a                \
	 ./zlib/libz.a                     \
	 ./aes/aes.o                       \
	 ./lua-cjson/lua_cjson.o           \
	 ./lua-cjson/strbuf.o              \
	 ./lua-cjson/fpconv.o              \
	 ./sqlite/sqlite3.o                \
	 -Wl,-Bdynamic,-ldl                \
	 -DWEB2                            \
	 -I ./lua/src                      \
	 -I ./leveldb/include              \
	 -I ./lwan/common                  \
	 -I ./zlib                         \
	 -I ./aes                          \
	 -I ./                             \
	 -std=c++0x                        \
	 -Wl,-Bstatic,-ljpeg               \
	 -Wl,-Bstatic,-lssl                \
	 -Wl,-Bstatic,-lcrypto             \
	 -Wl,-Bstatic,-lgcc                \
	 -o ../build/YRSSF
./zlib/libz.a :
	cd zlib       && make
./lwan/build/common/liblwan.a:
	-mkdir core/lwan/build
	cd core/lwan/build && cmake ../   && make	
./leveldb/out-static/libleveldb.a :
	cd leveldb    && make
./aes/aes.o :
	cd aes   && make
./lua/src/liblua.a :
	cd lua && make linux
sqlite/sqlite3.o : sqlite/sqlite3.c
	cd sqlite && gcc sqlite3.c -c
scriptqueue.hpp.gch : scriptqueue.hpp
	g++ scriptqueue.hpp -std=c++0x
key.hpp.gch : key.hpp
	g++ key.hpp
rwmutex.hpp.gch : rwmutex.hpp
	g++ rwmutex.hpp
base64.hpp.gch : base64.hpp
	g++ base64.hpp
lwan-build-config.h.gch : lwan-build-config.h
	g++ lwan-build-config.h
cache.hpp.gch : cache.hpp
	g++ cache.hpp -std=c++0x
classes.hpp.gch : classes.hpp
	g++ classes.hpp -std=c++0x
client.hpp.gch : client.hpp
	g++ client.hpp -I ./leveldb/include -std=c++0x
scriptworker.hpp.gch : scriptworker.hpp
	g++ scriptworker.hpp -std=c++0x
crypt.hpp.gch : crypt.hpp
	g++ crypt.hpp -std=c++0x
daemon.hpp.gch : daemon.hpp
	g++ daemon.hpp -I ./leveldb/include -std=c++0x
func.hpp.gch : func.hpp
	g++ func.hpp -std=c++0x
global.hpp.gch : global.hpp
	g++ global.hpp -std=c++0x
live.hpp.gch : live.hpp
	g++ live.hpp -I ./leveldb/include -std=c++0x
nint.hpp.gch : nint.hpp
	g++ nint.hpp -std=c++0x
server-base.hpp.gch : server-base.hpp
	g++ server-base.hpp -std=c++0x
server.hpp.gch : server.hpp
	g++ server.hpp -I ./leveldb/include -std=c++0x
webserver.hpp.gch : webserver.hpp
	g++ webserver.hpp -I ./lwan/common -I ./leveldb/include -std=c++0x
ysconnection.hpp.gch : ysconnection.hpp
	g++ ysconnection.hpp -I ./leveldb/include -std=c++0x
ysdb.hpp.gch : ysdb.hpp
	g++ ysdb.hpp -I ./leveldb/include -std=c++0x
lua-cjson/lua_cjson.o :
	cd lua-cjson && make
languagesolver.hpp.gch : languagesolver.hpp
	g++ languagesolver.hpp -std=c++0x
threadpool.hpp.gch : threadpool.hpp
	g++ threadpool.hpp -std=c++0x
clean:
	-rm *.gch
	-rm ../build/YRSSF
	-rm sqlite/sqlite3.o
	cd zlib      && make clean
	cd aes       && make clean
	cd lua       && make clean
	cd lua-cjson && make clean
	cd leveldb   && make clean
	-rm -rf lwan/build