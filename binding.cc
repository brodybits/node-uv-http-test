#include <nan_object_wrap_template.h>
#include <iostream>
#include <sstream>

#include <tr1/unordered_map>

// made with *some* help from:
// - https://github.com/brodybits/nan/blob/cb-object-wrapper-template/test/cpp/wrappedobjectfactory.cpp
//   which is based on:
//   https://github.com/nodejs/nan/blob/master/test/cpp/objectwraphandle.cpp

struct PathInfo {
  PathInfo(const char * path, int code, const char * content) :
    isRoute(true), path(path), code(code), content(content), hasCB(false) {}

  // XXX TODO: should store & use V8 isolate value:
  PathInfo(v8::Local<v8::Function> & f) : isRoute(true), pf(f), hasCB(true) {}

  PathInfo(const PathInfo & rhs) : isRoute(rhs.isRoute),
    hasCB(rhs.hasCB), path(rhs.path), code(rhs.code),
    content(rhs.content), pf(Nan::New(rhs.pf)) {}

  PathInfo() : isRoute(false), hasCB(false), code(404) {}

  PathInfo & operator=(PathInfo & rhs) {
    isRoute = rhs.isRoute;
    hasCB = rhs.hasCB;
    path = rhs.path;
    code = rhs.code;
    content = rhs.content;
    pf.Reset(Nan::New(rhs.pf));
    return *this;
  }

  bool isRoute;
  bool hasCB;

  std::string path;
  int code;
  std::string content;

  Nan::Persistent<v8::Function> pf;
};

/*
struct StaticPathInfo : public PathInfo {
  StaticPathInfo(const char * path, int code, const char * content) :
    PathInfo(path, code, content) {}
};

struct PathCBInfo : public PathInfo {
  // XXX TODO: should store & use V8 isolate value:
  PathCBInfo(v8::Local<v8::Function> & f) : PathInfo(f) {}
};
*/

typedef PathInfo StaticPathInfo;

typedef PathInfo PathCBInfo;

class HTTPServerReq : public ObjectWrapTemplate<HTTPServerReq> {
public:
  static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE ignored) {
    function_template tpl =
      NewConstructorFunctionTemplate("HTTPServerReq", 1);
    SetPrototypeMethod(tpl, "res", res);
    SetConstructorFunctionTemplate(tpl);
  }

  HTTPServerReq(Nan::NAN_METHOD_ARGS_TYPE args_info) : info(NULL) {} //, r(NULL) {}

  ~HTTPServerReq() {}

  static v8::Local<v8::Value> NewInstance(int argc, v8::Local<v8::Value> argv[]) {
    return NewInstanceMethod(argc, argv);
  }

  static void res(Nan::NAN_METHOD_ARGS_TYPE args_info) {
    HTTPServerReq * myself = ObjectFromMethodArgsInfo(args_info);

    if (args_info.Length() < 2 ||
        !args_info[0]->IsInt32() ||
        !args_info[1]->IsString()) {
      std::cerr << "Sorry incorrect arguments to response function" << std::endl;
      return;
    }

    std::string cs (*v8::String::Utf8Value(args_info[1]->ToString()));

    //uv_write_t mywrite;
    uv_write_t * writehandle = new uv_write_t;
    //std::string resp("HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nabc\n");
    std::stringstream sst;
    // XXX TODO FIX RESPONSE CODE
    sst << "HTTP/1.1 200 OK\r\nContent-Length: " << cs.length() <<
        "\r\n\r\n" << cs;
    std::string resp(sst.str());
    uv_buf_t mybuf;
    mybuf.base = (char *)(resp.c_str());
    mybuf.len = resp.length();
    //uv_write(&mywrite, myself->s, &mybuf, 1, NULL);
    uv_write(writehandle, myself->s, &mybuf, 1, writecb);
  }

  static void writecb(uv_write_t * w, int) {
    delete w;
  }

  PathCBInfo * info;

  uv_stream_t * s;
};

class HTTPServer : public ObjectWrapTemplate<HTTPServer> {
public:
  static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE ignored) {
    function_template tpl =
      NewConstructorFunctionTemplate("HTTPServer", 1);
    SetPrototypeMethod(tpl, "staticPath", StaticPath);
    SetPrototypeMethod(tpl, "pathCB", PathCB);
    SetPrototypeMethod(tpl, "bindSocket", BindSocket);
    SetPrototypeMethod(tpl, "bindAddr", BindAddr);
    SetConstructorFunctionTemplate(tpl);
  }

  HTTPServer(Nan::NAN_METHOD_ARGS_TYPE args_info) {
    /*
    */
  }

  ~HTTPServer() {}

  static void NewInstance(Nan::NAN_METHOD_ARGS_TYPE args_info) {
    NewInstanceMethod(args_info);
  }

  static v8::Local<v8::Value> NewInstance(int argc, v8::Local<v8::Value> argv[]) {
    return NewInstanceMethod(argc, argv);
  }

  static void StaticPath(Nan::NAN_METHOD_ARGS_TYPE args_info) {
    HTTPServer * myself = ObjectFromMethodArgsInfo(args_info);

    if (args_info.Length() < 3 ||
        !args_info[0]->IsString() ||
        !args_info[1]->IsInt32() ||
        !args_info[2]->IsString()) {
      std::cerr << "Sorry incorrect arguments to staticPath" << std::endl;
      return;
    }

    std::string mypath(*v8::String::Utf8Value(args_info[0]->ToString()));

    // XXX TBD ???:
    if (mypath[0] != '/') {
      std::cerr << "Sorry invalid path" << std::endl;
      return;
    }

    // XXX TODO:
    // - Support true Buffer(s)
    // - TBD ???: free the memory in case this static path is no longer relevant
    // - TBD ???: get rid of extra std::string storage to make this more efficient
    std::string mycontent(*v8::String::Utf8Value(args_info[2]->ToString()));

    PathInfo pi(mypath.c_str(), args_info[1]->Int32Value(), mycontent.c_str());
    myself->routes[mypath] = pi;
  }

  static void PathCB(Nan::NAN_METHOD_ARGS_TYPE args_info) {
    HTTPServer * myself = ObjectFromMethodArgsInfo(args_info);

    if (args_info.Length() < 2 ||
        !args_info[0]->IsString() ||
        !args_info[1]->IsFunction()) {
      std::cerr << "Sorry incorrect arguments to pathCB()" << std::endl;
      return;
    }

    std::string mypath(*v8::String::Utf8Value(args_info[0]->ToString()));

    // XXX TBD ???:
    if (mypath[0] != '/') {
      std::cerr << "Sorry invalid path" << std::endl;
      return;
    }

    // XXX TODO:
    // - Support true Buffer(s)
    // - TBD ???: free the memory in case this static path is no longer relevant
    // - TBD ???: get rid of extra std::string storage to make this more efficient
    v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(args_info[1]);
    //PathCBInfo * info = new PathCBInfo(f);

    PathCBInfo pi(f);
    myself->routes[mypath] = pi;
  }

  static void AllocForRead(uv_handle_t *, size_t s, uv_buf_t * b) {
    uint8_t * mybuf = new uint8_t[s];
    b->base = reinterpret_cast<char *>(mybuf);
    b->len = s;
  }

  static void TestRead(uv_stream_t * s, ssize_t n, const uv_buf_t * b) {
    std::cout << "read cb n: " << n << std::endl;

    // XXX TODO [MISSING]:
    // - must wait for entire HTTP request
    // - error checking
    // (use @nodejs/http-parser instead to solve these)

    if (n < 0) return;

    HTTPServer * myself = reinterpret_cast<HTTPServer *>(s->data);

    std::string url;

    if (n > 0) {
      std::string s((const char *)b->base, n);
      std::cout << "read: " << s << std::endl;
      if (s.length() >= 5 && s.substr(0, 5) == "GET /") {
        //int p2 = s.find(4, '/');
        int p2 = 4;
        while (p2 < n && s[p2] != ' ') ++p2;
        std::cout << "p2: " << p2 << std::endl;
        if (p2 != std::string::npos) {
          //url = s.substr(4, p2);
          url = s.substr(4, p2-4);
          std::cout << "Got url: " << url << std::endl;
        }
      }
    }

    auto found = myself->routes.find(url);

    // XXX UGLY (TODO CLEANUP)
    if (found != myself->routes.end() && found->second.isRoute) {
      //std::cout << "found" << std::endl;
      if (found->second.hasCB) {
        Nan::HandleScope myscope;

        Nan::Callback cb(Nan::New(found->second.pf));

        // XXX UGLY (TODO CLEANUP)
        v8::Local<v8::Value> sr_argv[1] = {Nan::New(1)};
        v8::Local<v8::Object> sr = HTTPServerReq::NewInstance(1, sr_argv)->ToObject();

        HTTPServerReq * mysr = ObjectWrap::Unwrap<HTTPServerReq>(sr);
        // XXX VERY UGLY
        mysr->info = &found->second;
        mysr->s = s;

        static int argc = 1;
        v8::Local<v8::Value> argv[1] = {sr};

        cb.Call(argc, argv);
      } else {
        // XXX UGLY (TODO CLEANUP)
        std::cout << "found static" << std::endl;
        std::cout << "with code: " << found->second.code << std::endl;
        uv_write_t mywrite;
        //std::string resp("HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nabc\n");
        std::stringstream sst;
        // XXX TODO FIX RESPONSE CODE
        sst << "HTTP/1.1 200 OK\r\nContent-Length: " << found->second.content.length() <<
            "\r\n\r\n" << found->second.content;
        std::string resp(sst.str());
        uv_buf_t mybuf;
        mybuf.base = (char *)(resp.c_str());
        mybuf.len = resp.length();
        uv_write(&mywrite, s, &mybuf, 1, NULL);
      }
    } else {
      uv_write_t mywrite;
      std::string resp("HTTP/1.1 404 Not Found\r\nContent-Length: 16\r\n\r\nSorry not found\n");
      uv_buf_t mybuf;
      mybuf.base = (char *)(resp.c_str());
      mybuf.len = resp.length();
      uv_write(&mywrite, s, &mybuf, 1, NULL);
    }
  }

  static void HandleNewConnection(uv_stream_t *s, int st) {
    if (st < 0) {
      std::cerr << "new connection with error status: " << st << std::endl;
      return;
    }

    uv_tcp_t * tcpin = new uv_tcp_t; // XXX TODO LEAK - keep in a struct instead!

    uv_tcp_init(uv_default_loop(), tcpin);
    tcpin->data = s->data;

    int r = uv_accept(s, (uv_stream_t *)tcpin);
    if (r != 0) {
      uv_close(reinterpret_cast<uv_handle_t *>(tcpin), NULL);
      return;
    }
    uv_read_start(reinterpret_cast<uv_stream_t *>(tcpin), AllocForRead, TestRead);
  }

  static void BindSocket(Nan::NAN_METHOD_ARGS_TYPE args_info) {
    HTTPServer * myself = ObjectFromMethodArgsInfo(args_info);

    if (args_info.Length() < 3 ||
        !args_info[0]->IsString() ||
        !args_info[1]->IsInt32() ||
        !args_info[2]->IsInt32()) {
      std::cerr << "Sorry incorrect arguments to bindSocket" << std::endl;
      return;
    }
  }

  static void BindAddr(Nan::NAN_METHOD_ARGS_TYPE args_info) {
    HTTPServer * myself = ObjectFromMethodArgsInfo(args_info);

    if (args_info.Length() < 3 ||
        !args_info[0]->IsString() ||
        !args_info[1]->IsInt32()) {
      std::cerr << "Sorry incorrect arguments to bindAddr" << std::endl;
      return;
    }

    // *some* help from: https://nikhilm.github.io/uvbook/networking.html
    struct sockaddr_in myaddr;

    uv_tcp_init(uv_default_loop(), &myself->mytcphandle);
    myself->mytcphandle.data = myself;
    // XXX TODO TODO IP 6:
    uv_ip4_addr(*v8::String::Utf8Value(args_info[0]->ToString()),
                args_info[1]->Int32Value(), &myaddr);
    uv_tcp_bind(&myself->mytcphandle, reinterpret_cast<const sockaddr *>(&myaddr), 0);

    int res = uv_listen(reinterpret_cast<uv_stream_t *>(&myself->mytcphandle),
                        1024, HandleNewConnection);
    if (res != 0) {
      std::cerr << "sorry listen error: " << res << std::endl;
    }
  }

/*
  static void uvtest(Nan::NAN_METHOD_ARGS_TYPE args_info) {
    // *some* help from: https://nikhilm.github.io/uvbook/networking.html
    // XXX TODO TODO:
    static uv_tcp_t mytcp;
    struct sockaddr_in myaddr;

    std::cout << "start uvtest" << std::endl;
    uv_tcp_init(uv_default_loop(), &mytcp);
    uv_ip4_addr("0.0.0.0", 8000, &myaddr);
    uv_tcp_bind(&mytcp, reinterpret_cast<const sockaddr *>(&myaddr), 0);
    std::cout << "start listen" << std::endl;
    int res = uv_listen(reinterpret_cast<uv_stream_t *>(&mytcp), 1024, HandleNewConnection);
    if (res != 0) {
      std::cerr << "sorry listen error: " << res << std::endl;
    }
    std::cout << "finished uvtest function" << std::endl;
  }
*/

  // XXX TODO support closing

  // XXX TODO use something better such as @c9s/r3 instead (!)
  std::tr1::unordered_map<std::string, PathInfo> routes;

  // FUTURE TBD ???: mTCP - user-space TCP
  uv_tcp_t mytcphandle;
};

/*
void MyEventServer::NewHTTPServer(Nan::NAN_METHOD_ARGS_TYPE args_info) {
  v8::Local<v8::Value> lv = args_info.This();
  v8::Local<v8::Value> argv[1] = { lv };
  args_info.GetReturnValue().Set(HTTPServer::NewInstance(1, argv));
}
*/

void init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  HTTPServerReq::Init(target);
  HTTPServer::Init(target);
  Nan::Set(target, Nan::New<v8::String>("newHTTPServer").ToLocalChecked(),
           Nan::New<v8::FunctionTemplate>(HTTPServer::NewInstance)->GetFunction());
}

NODE_MODULE(evhtp, init)
