#include <pthread.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <memory>
extern "C" {
#include "lua/src/lua.h"
#include "lua/src/lualib.h"
#include "lua/src/lauxlib.h"
#include "lua/src/luaconf.h"
#include "zlib.h"
}
#include "base64.hpp"
#include "rwmutex.hpp"
#include "key.hpp"
#include <cassert>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <string.h>
#include <map>
#include <set>
#include <stack>
#include <list>
#include <math.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/prctl.h>
#include <dlfcn.h>
#include <sqlite3.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <jpeglib.h>
#include <jerror.h>
#include "func.hpp"
#include "nint.hpp"
#include "classes.hpp"
#include "crypt.hpp"
#include "ysdb.hpp"
#include "server-base.hpp"
#include "ysconnection.hpp"
#include "server.hpp"
#include "client.hpp"
#include "daemon.hpp"
#include "live.hpp"
#include "webserver.hpp"
#include "scriptqueue.hpp"
#include "scriptworker.hpp"
#include "cache.hpp"
#include "languagesolver.hpp"
extern "C" int luaopen_cjson(lua_State *l);
namespace yrssf{
///////////////////////////////////
sqlite3  * db;
std::mutex lualocker;
class Lglobal{
  public:
  std::map<std::string,std::string> m;
  RWMutex locker;
  Lglobal(){}
  ~Lglobal(){}
  int set(lua_State * L){
    if(!lua_isstring(L,1))return 0;
    if(!lua_isstring(L,2))return 0;
    locker.Wlock();
    m[lua_tostring(L,1)]=lua_tostring(L,2);
    locker.unlock();
    return 0;
  }
  int read(lua_State * L){
    if(!lua_isstring(L,1))return 0;
    locker.Rlock();
    auto it=m.find(lua_tostring(L,1));
    if(it==m.end()){
      locker.unlock();
      return 0;
    }
    std::string & s=it->second;
    lua_pushstring(L,s.c_str());
    locker.unlock();
    return 1;
  }
  int del(lua_State * L){
    if(!lua_isstring(L,1))return 0;
    locker.Wlock();
    auto it=m.find(lua_tostring(L,1));
    if(it!=m.end())
      m.erase(it);
    locker.unlock();
    return 0;
  }
}lglobal;
class API{
  int shmid;
  public:
  API(){
    db=NULL;
    ysDebug("\033[40;43mYRSSF:\033[0mdatabase loading...\n");
    sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
    int ret = sqlite3_open("./data/yrssf.db", &db);
    if( ret != SQLITE_OK ) {
      ysDebug("can not open %s \n", sqlite3_errmsg(db));
      return;
    }
    signal(10,[](int){
      client.liveclientrunning=1;
      runliveclient();
    });
    signal(12,[](int){
      client.liveclientrunning=0;
      clientdisabled=0;
    });
    mkdir("./live",0777);
    livefifo=mkfifo("./live/client",0666);
    liveserverfifo=mkfifo("./live/server",0666);
  }
  ~API(){
    if(db)sqlite3_close(db);
    close(livefifo);
    close(liveserverfifo);
    pool.del(liveserverreq);
  }
  static int pathfilter(lua_State * L){
    char *ostr;
    const char * res;
    if(!lua_isstring(L,1)) return 0;
    res=lua_tostring(L,1);
    ostr=(char*)malloc(strlen(res)+1);
    int i=0;
    while(*res){
      ostr[i]=*res;
      if(ostr[i]=='/') ostr[i]='.';
      i++;
    }
    ostr[i]='\0';
    lua_pushstring(L,ostr);
    free(ostr);
    return 1;
  }
  static int sqlfilter(lua_State * L){
    const static char reps[]={'\'','"','%','\0'};
    const char * istr;
    char *ostr,*sp2;
    const char * sp;
    int len,i;
    if(!lua_isstring(L,1))return 0;
    istr=lua_tostring(L,1);
    len=strlen(istr);
    ostr=(char*)malloc(len*2 + 1);
    sp2=ostr;
    for(i=0;i<len;i++){
      sp=reps;
      while(*sp){
        if(istr[i]==*sp){
          *sp2='\\';
          sp2++;
          goto end;
        }
        sp++;
      }
      end:
      *sp2=istr[i];
      sp2++;
    }
    *sp2='\0';
    lua_pushstring(L,ostr);
    free(ostr);
    return 1;
  }
  struct runsqlcbs{
    lua_State * lua;
    int i;
  };
  static int runsql_callback(
    void * d,
    int col_count,
    char ** col_values,
    char ** col_Name
  ){
    runsqlcbs * data=(runsqlcbs*)d;
    lua_State * L=data->lua;
    lua_createtable(L,col_count,0);//create new array
    lua_pushnumber(L,-1);
    lua_rawseti(L,-2,0);  //fill array[i][0]
    for(int i=0;i<col_count;i++){
      lua_pushstring(L,col_values[i]);
      lua_rawseti(L,-2,i+1);
    }
    lua_rawseti(L,-2,data->i);
    data->i++;
    return 0;
  }
  static int runsql(lua_State * L){
    char * pErrMsg;
    int ret;
    if(!lua_isstring(L,1))return 0;
    const char * sSQL1=lua_tostring(L,1);
    lua_newtable(L);//create main array
    lua_pushnumber(L,-1);
    lua_rawseti(L,-2,0);  //fill array[0]
    runsqlcbs d;
    d.lua=L;
    d.i=1;
    ret=sqlite3_exec( db, sSQL1, runsql_callback, &d, &pErrMsg );
    if( ret != SQLITE_OK ){
      lua_pop(L,1);
      lua_pushstring(L,pErrMsg);
      lua_pushboolean(L,0);
      sqlite3_free(pErrMsg);
      return 2;
    }else{
      lua_pushboolean(L,1);
      return 2;
    }
  }
  static int lua_upload(lua_State * L){
      if(clientdisabled)return 0;
      char str[9];
      str[8]='\0';
      if(!lua_isstring(L,1))return 0;
      if(!lua_isstring(L,2))return 0;
      const char * sname=lua_tostring(L,1);
      for(int i=0;i<8;i++){
        str[i]=sname[i];
        if(sname[i]=='\0')
          break;
      }
      clientlocker.lock();
      client.upload(str,lua_tostring(L,2));
      clientlocker.unlock();
      return 0;
  }
  static int lua_download(lua_State * L){
      if(clientdisabled)return 0;
      char str[9];
      str[8]='\0';
      if(!lua_isstring(L,1))return 0;
      if(!lua_isstring(L,2))return 0;
      const char * sname=lua_tostring(L,1);
      for(int i=0;i<8;i++){
        str[i]=sname[i];
        if(sname[i]=='\0')
          break;
      }
      clientlocker.lock();
      client.download(str,lua_tostring(L,2));
      clientlocker.unlock();
      return 0;
  }
  static int lua_del(lua_State * L){
      if(clientdisabled)return 0;
      char str[9];
      str[8]='\0';
      if(!lua_isstring(L,1))return 0;
      const char * sname=lua_tostring(L,1);
      for(int i=0;i<8;i++){
        str[i]=sname[i];
        if(sname[i]=='\0')
          break;
      }
      clientlocker.lock();
      client.del(str);
      clientlocker.unlock();
      return 0;
  }
  static int lua_setPwd(lua_State * L){
      if(clientdisabled)return 0;
      if(lua_isstring(L,1)){
        if(!lua_isstring(L,2))return 0;
        clientlocker.lock();
        client.setPwd(lua_tostring(L,1),lua_tostring(L,2));
        clientlocker.unlock();
        return 0;
      }else
      if(lua_isinteger(L,1)){
        if(!lua_isstring(L,2))return 0;
        clientlocker.lock();
        client.setPwd(lua_tostring(L,2),(int32_t)lua_tointeger(L,1));
        clientlocker.unlock();
        return 0;
      }
  }
  static int lua_newUser(lua_State * L){
      if(clientdisabled)return 0;
      if(!lua_isinteger(L,1))return 0;
      clientlocker.lock();
      client.newUser(lua_tointeger(L,1));
      clientlocker.unlock();
      return 0;
  }
  static int lua_cps(lua_State * L){//change parent server
      if(clientdisabled)return 0;
      if(!lua_isstring (L,1))return 0;
      if(!lua_isinteger(L,2))return 0;
      in_addr addr;
      addr.s_addr=inet_addr(lua_tostring(L,1));
      clientlocker.lock();
      lua_pushboolean(L,client.changeParentServer(addr,lua_tointeger(L,2)));
      clientlocker.unlock();
      return 1;
  }
  static int lua_ssu(lua_State * L){
      if(!lua_isstring (L,2))return 0;
      if(!lua_isinteger(L,1))return 0;
      server.myuserid=lua_tointeger(L,1);
      wristr(lua_tostring(L,2),server.mypassword);
      server.login();
      return 0;
  }
  static int lua_scu(lua_State * L){
      if(clientdisabled)return 0;
      if(!lua_isstring (L,2))return 0;
      if(!lua_isinteger(L,1))return 0;
      client.myuserid=lua_tointeger(L,1);
      wristr(lua_tostring(L,2),client.mypassword);
      client.login();
      return 0;
  }
  static int lua_connectUser(lua_State * L){
    in_addr addr;
    if(!lua_isinteger(L,1))return 0;
    if(!lua_isstring (L,2))return 0;
    if(!lua_isinteger(L,3))return 0;
    addr.s_addr=inet_addr(lua_tostring(L,2));
    server.connectUser(lua_tointeger(L,1),addr,lua_tointeger(L,3));
    return 0;
  }
  static int lua_connectToUser(lua_State * L){
    if(clientdisabled)return 0;
    in_addr addr;
    short   port;
    if(!lua_isinteger(L,1))return 0;
    clientlocker.lock();
    lua_pushboolean(L,(client.connectToUser(lua_tointeger(L,1),&addr,&port)));
    clientlocker.unlock();
    char ipbuf[32];
    inttoip(addr,ipbuf);
    lua_pushstring(L,ipbuf);
    lua_pushinteger(L,port);
    return 3;
  }
  static int lua_setkey(lua_State * L){
    if(clientdisabled)return 0;
    aesblock key;
    bzero(&key,sizeof(key));
    const char * str;
    if(!lua_isstring(L,1))return 0;
    str=lua_tostring(L,1);
    for(int i=0;(i<16 && str[i]);i++)
      key.data[i]=str[i];
    if(!lua_isinteger(L,2)){
      client.key=key;
    }else{
      ysDB.setkey(lua_tointeger(L,2),&key);
    }
    return 0;
  }
  static int lua_getkey(lua_State * L){
    aesblock key;
    char str[17];
    str[16]='\0';
    int i;
    aesblock *tk;
    if(!lua_isinteger(L,1)){
      tk=&(client.key);
    }else{
      tk=&key;
      ysDB.getkey(lua_tointeger(L,1),&key);
    }
    for(i=0;i<16;i++) str[i]=tk->data[i];
    lua_pushstring(L,str);
    return 1;
  }
  void funcreg(lua_State * L){
    lua_register(L,"clientUpload",        lua_upload);
    lua_register(L,"sqlfilter",           sqlfilter);
    lua_register(L,"clientDownload",      lua_download);
    lua_register(L,"clientDel",           lua_del);
    lua_register(L,"clientSetPwd",        lua_setPwd);
    lua_register(L,"clientNewUser",       lua_newUser);
    lua_register(L,"changeParentServer",  lua_cps);
    lua_register(L,"connectUser",         lua_connectUser);
    lua_register(L,"connectToUser",       lua_connectToUser);
    lua_register(L,"getkey",              lua_getkey);
    lua_register(L,"setkey",              lua_setkey);
    lua_register(L,"runsql",              runsql);
    lua_register(L,"cache_read",          cache::read);
    lua_register(L,"cache_set",           cache::set);
    lua_register(L,"cache_delete",        cache::del);
    lua_register(L,"worker",              sworker::create);
    /*
    lua_register(L,"gethostbyname",[](lua_State * L){
      if(!lua_isstring(L,1)) return 0;
      static std::mutex lk;
      lk.lock();
      lua_pushstring(L,
        inet_ntcp(
          gethostbyname(lua_tostring(L,1))->h_addr_list
        )
      );
      lk.unlock();
      return 1;
    });
    */
    lua_register(L,"nodeModeOn",[](lua_State * L){
      config::nodemode=1;
      return 0;
    });
    lua_register(L,"nodeModeOff",[](lua_State * L){
      config::nodemode=0;
      return 0;
    });
    lua_register(L,"GLOBAL_read",[](lua_State * L){
      return lglobal.read(L);
    });
    lua_register(L,"GLOBAL_set",[](lua_State * L){
      return lglobal.set(L);
    });
    lua_register(L,"GLOBAL_delete",[](lua_State * L){
      return lglobal.del(L);
    });
    lua_register(L,"hex2num",[](lua_State * L){
      if(!lua_isstring(L,1)) return 0;
      const char * str=lua_tostring(L,1);
      int num;
      str2int(str,&num);
      lua_pushinteger(L,num);
      return 1;
    });
    lua_register(L,"insertIntoQueue",[](lua_State * L){
      if(!lua_isstring(L,1)) return 0;
      scriptqueue.insert(lua_tostring(L,1));
      return 0;
    });
    lua_register(L,"delUser",[](lua_State * L){
      if(!lua_isinteger(L,1))return 0;
      ysDB.delUser(lua_tointeger(L,1));
      return 0;
    });
    lua_register(L,"ysThreadLock",[](lua_State * L){
      lualocker.lock();
      return 0;
    });
    lua_register(L,"ysThreadUnlock",[](lua_State * L){
      lualocker.unlock();
      return 0;
    });
    lua_register(L,"signerInit",[](lua_State * L){
      if(lua_isstring(L,1) && lua_isstring(L,2)){
        signer.init(lua_tostring(L,1),lua_tostring(L,2));
      }else{
        signer.init();
      }
      lua_pushstring(L,(const char*)signer.pubkey);
      lua_pushstring(L,(const char*)signer.privkey);
      return 2;
    });
    lua_register(L,"clientGetUniKey",[](lua_State * L){
      if(clientdisabled)return 0;
      clientlocker.lock();
      lua_pushboolean(L,client.getUniKey());
      clientlocker.unlock();
      return 1;
    });
    lua_register(L,"clientRegister",[](lua_State * L){
      if(clientdisabled)return 0;
      if(!lua_isstring(L,1)){
        clientlocker.lock();
        lua_pushboolean(L,client.reg(&myUniKey));
        clientlocker.unlock();
      }else{
        clientlocker.lock();
        lua_pushboolean(L,client.reg(lua_tostring(L,1)));
        clientlocker.unlock();
      }return 1;
    });
    lua_register(L,"addSignKey",[](lua_State * L){
      if(!lua_isstring(L,1))return 0;
      signer.add(lua_tostring(L,1));
      return 0;
    });
    lua_register(L,"checkSignOn",[](lua_State * L){
      config::checkSign=1;
      return 0;
    });
    lua_register(L,"checkSignOff",[](lua_State * L){
      config::checkSign=0;
      return 0;
    });
    lua_register(L,"logUnique",[](lua_State * L){
      if(!lua_isinteger(L,1))return 0;
      if(!lua_isinteger(L,2))return 0;
      lua_pushboolean(L,ysDB.logunique(lua_tointeger(L,1),lua_tointeger(L,2)));
      return 1;
    });
    lua_register(L,"uniqueExist",[](lua_State * L){
      if(!lua_isinteger(L,1))return 0;
      if(!lua_isinteger(L,2))return 0;
      lua_pushboolean(L,ysDB.uniqueExist(lua_tointeger(L,1),lua_tointeger(L,2)));
      return 1;
    });
    lua_register(L,"clientSrcInit",[](lua_State * L){
      if(!lua_isstring(L,1))return 0;
      if(clientdisabled)return 0;
      clientlocker.lock();
      client.newSrc(lua_tostring(L,1));
      clientlocker.unlock();
      return 0;
    });
    lua_register(L,"allowShell",[](lua_State * L){
      config::AllowShell=1;
      return 0;
    });
    lua_register(L,"disallowShell",[](lua_State * L){
      config::AllowShell=0;
      return 0;
    });
    lua_register(L,"boardcastSoundInit",[](lua_State * L){
      if(!lua_isstring(L,1))return 0;
      videolive::screen.initSound(lua_tostring(L,1));
      return 0;
    });
    lua_register(L,"boardcastSound",[](lua_State * L){
      videolive::screen.sendsound();
      return 0;
    });
    lua_register(L,"boardcastSoundDestory",[](lua_State * L){
      videolive::screen.destorySound();
      return 0;
    });
    lua_register(L,"boardcastScreenInit",[](lua_State * L){
      if(!lua_isstring(L,1))return 0;
      videolive::screen.init(lua_tostring(L,1));
      return 0;
    });
    lua_register(L,"boardcastScreen",[](lua_State * L){
      videolive::screen.sendall();
      return 0;
    });
    lua_register(L,"liveScreen",[](lua_State * L){
      videolive::screen.liveall();
      return 0;
    });
    lua_register(L,"screenShot",[](lua_State * L){
      if(!lua_isstring(L,1))return 0;
      videolive::screen.save(lua_tostring(L,1));
      return 0;
    });
    lua_register(L,"boardcastScreenDestory",[](lua_State * L){
      videolive::screen.destory();
      return 0;
    });
    lua_register(L,"canQuery",[](lua_State * L){
      if(clientdisabled){
        lua_pushboolean(L,0);
        return 1;
      }
      if(clientlocker.try_lock()){
        clientlocker.unlock();
        lua_pushboolean(L,1);
      }else{
        lua_pushboolean(L,0);
      }
      return 1;
    });
    lua_register(L,"updatekey",[](lua_State * L){
      if(clientdisabled)return 0;
      clientlocker.lock();
      lua_pushboolean(L,client.updatekey());
      clientlocker.unlock();
      return 1;
    });
    lua_register(L,"globalModeOn",[](lua_State * L){
      client.globalmode='t';
      return 0;
    });
    lua_register(L,"globalModeOff",[](lua_State * L){
      client.globalmode='f';
      return 0;
    });
    lua_register(L,"cryptModeOn",[](lua_State * L){
      client.iscrypt=1;
      return 0;
    });
    lua_register(L,"cryptModeOff",[](lua_State * L){
      client.iscrypt=0;
      return 0;
    });
    lua_register(L,"liveadd",[](lua_State * L){
      location ad;
      if(!lua_isstring(L,1))return 0;
      ad.ip.s_addr=inet_addr(lua_tostring(L,1));
      if(!lua_isinteger(L,2))return 0;
      ad.port=lua_tointeger(L,2);
      ysDB.liveAdd(ad);
      return 0;
    });
    lua_register(L,"liveclean",[](lua_State * L){
      ysDB.liveClean();
      return 0;
    });
    lua_register(L,"liveModeOn",[](lua_State * L){
      if(clientdisabled)return 0;
      client.liveclientrunning=1;
      runliveclient();
      return 0;
    });
    lua_register(L,"liveModeOff",[](lua_State * L){
      client.liveclientrunning=0;
      clientdisabled=0;
      return 0;
    });
    lua_register(L,"setServerUser",lua_ssu);
    lua_register(L,"setClientUser",lua_scu);
    luaopen_cjson(L);
    langsolver::luaopen(L);
    luaopen_ysfunc(L);
  }
}api;
class Init{
  public:
  static void* runServerThread(void*){
    server.runServer();
  }
  void run(){
    pthread_t newthread;
    ysDebug("\033[40;43mYRSSF:\033[0m");
    ysDebug("\033[40;36mYRSSF create thread\033[0m\n");
    if(pthread_create(&newthread,NULL,runServerThread,NULL)!=0)
      perror("pthread_create");
  }
  Init(){
    pthread_t newthread;
    char sbuffer[PATH_MAX];
    getcwd(sbuffer,PATH_MAX);
    if(pthread_create(&newthread,NULL,liveserver,NULL)!=0)
      perror("pthread_create");
    ysDebug("\033[40;43mYRSSF:\033[0m\033[40;36mYRSSF init\033[0m\n");
    gblua=luaL_newstate();
    luaL_openlibs(gblua);
    lua_pushinteger(gblua,(int)&server);
    lua_setglobal(gblua,"_SERVER");
    lua_pushinteger(gblua,(int)&client);
    lua_setglobal(gblua,"_CLIENT");
    lua_pushstring(gblua,sbuffer);
    lua_setglobal(gblua,"APP_PATH");
    ysConnection::funcreg(gblua);
    api.funcreg(gblua);
    scriptqueuerun();
    luaL_dofile(gblua,"init.lua");
    if(lua_isstring(gblua,-1)){
      std::cout<<lua_tostring(gblua,-1)<<std::endl;
    }
  }
  ~Init(){
    ysDebug("\033[40;43mYRSSF:\033[0m\033[40;36mYRSSF server shutdown\033[0m\n");
    server.shutdown();
    scriptqueue.stop();
    lua_close(gblua);
  }
}init;
///////////////////////////
}//////////////////////////
///////////////////////////
extern "C" void runServer(){
    yrssf::init.run();
}
extern "C" void shutdownServer(){
    ysDebug("shutdown");
    //yrssf::init.~Init();
}
extern "C" void loadAPI(lua_State * L){
    lua_pushinteger(L,(int)&yrssf::server);
    lua_setglobal(L,"SERVER");
    lua_pushinteger(L,(int)&yrssf::client);
    lua_setglobal(L,"CLIENT");
    yrssf::ysConnection::funcreg(L);
    yrssf::api.funcreg(L);
}
extern "C" int luaopen_yrssf(lua_State * L){
    loadAPI(L);
    return 1;
}
extern "C" void dofile(const char * path){
    lua_State * L=lua_newthread(yrssf::gblua);
    luaL_dofile(L,path);
}
int main(){
    ysDebug("init...");
    runServer();
    webserverRun();
    shutdownServer();
    return 0;
}
class Automanager{
  static void * freeunique(void*){
    while(1){
      sleep(60);
      int time;
      int nowtime=yrssf::nowtime_s();
      int num=0;
      leveldb::ReadOptions options;
      options.snapshot = yrssf::ysDB.unique->GetSnapshot();
      leveldb::Iterator* it = yrssf::ysDB.unique->NewIterator(options);
      for(it->SeekToFirst(); it->Valid(); it->Next()){
        time=atoi(it->value().ToString().c_str());
        if(abs(time-nowtime)<300) continue;
        yrssf::ysDB.unique->Delete(leveldb::WriteOptions(),it->key().ToString());
        num++;
      }
      delete it;
      yrssf::ysDB.unique->ReleaseSnapshot(options.snapshot);
      //ysDebug("free %d unique id",num);
    }
  }
  static void * freemems(void*){
    int num[24];
    int times[24];
    int nownum;
    int nowhour;
    int buf;
    int i;
    time_t timer;
    bzero(num,   sizeof(int)*24);
    bzero(times, sizeof(int)*24);
    while(1){
      sleep(3600);
      yrssf::cache::freetrash();
      timer=time(NULL);
      struct tm *ptr  =localtime(&timer);
      nowhour         =ptr->tm_hour;
      nownum          =num[nowhour];
      buf             =num[nowhour]*times[nowhour];
      times[nowhour]++;
      num[nowhour]    =(buf+yrssf::pool.mallocCount)/times[nowhour];
      if(nownum<64) continue;
      if(abs((float)yrssf::pool.usingCount/(float)yrssf::pool.mallocCount)>0.8f) continue;
      i=0;
      while(yrssf::pool.mallocCount>(nownum+32)){
        yrssf::pool.freeone();
        i++;
      }
      //ysDebug("free %d memory block",i);
    }
  }
  public:
  Automanager(){
    pthread_t newthread;
    if(pthread_create(&newthread,NULL,freeunique,NULL)!=0)
      perror("pthread_create");
    if(pthread_create(&newthread,NULL,freemems,NULL)!=0)
      perror("pthread_create");
  }
  ~Automanager(){
  }
}automanager;
