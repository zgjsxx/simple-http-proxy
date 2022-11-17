#ifndef SRS_APP_CONFIG_HPP
#define SRS_APP_CONFIG_HPP
#include <srs_app_reload.hpp>
#include <srs_core_time.hpp>
#include <srs_core.hpp>
#include <string>
#include <vector>
#include <string.h>
#define SRS_DEFAULT_CONFIG "conf/srs.conf"
class SrsConfig;

namespace srs_internal
{
    class SrsConfigBuffer
    {
    protected:
        // The last available position.
        char* last;
        // The end of buffer.
        char* end;
        // The start of buffer.
        char* start;
    public:
        // Current consumed position.
        char* pos;
        // Current parsed line.
        int line;
    public:
        SrsConfigBuffer();
        virtual ~SrsConfigBuffer();
    public:
        virtual srs_error_t fullfill(const char* name);
        virtual bool empty();
    };
};

class SrsConfDirective
{
public:
    // The line of config file in which the directive from
    int conf_line;
    // The name of directive, for example, the following config text:
    //       enabled     on;
    // will be parsed to a directive, its name is "enalbed"
    std::string name;
    // The args of directive, for example, the following config text:
    //       listen      1935 1936;
    // will be parsed to a directive, its args is ["1935", "1936"].
    std::vector<std::string> args;
    // The child directives, for example, the following config text:
    //       vhost vhost.ossrs.net {
    //           enabled         on;
    //       }
    // will be parsed to a directive, its directives is a vector contains
    // a directive, which is:
    //       name:"enalbed", args:["on"], directives:[]
    //
    // @remark, the directives can contains directives.
    std::vector<SrsConfDirective*> directives;
public:
    SrsConfDirective();
    virtual ~SrsConfDirective();  
public:
    // Get the args0,1,2, if user want to get more args,
    // directly use the args.at(index).
    virtual std::string arg0();
    virtual std::string arg1();
    virtual std::string arg2();
    virtual std::string arg3();
public:
    // Get the directive by index.
    // @remark, assert the index<directives.size().
    virtual SrsConfDirective* at(int index);
    // Get the directive by name, return the first match.
    virtual SrsConfDirective* get(std::string _name); 
    // Get the directive by name and its arg0, return the first match.
    virtual SrsConfDirective* get(std::string _name, std::string _arg0); 
public:
    virtual SrsConfDirective* get_or_create(std::string n);
    virtual SrsConfDirective* get_or_create(std::string n, std::string a0);
    virtual SrsConfDirective* get_or_create(std::string n, std::string a0, std::string a1);
    virtual SrsConfDirective* set_arg0(std::string a0);
    // Remove the v from sub directives, user must free the v.
    virtual void remove(SrsConfDirective* v);
public:
    // Whether current directive is vhost.
    virtual bool is_vhost();
    // Whether current directive is stream_caster.
    virtual bool is_stream_caster();    
public:
// Parse utilities
public:
    // Parse config directive from file buffer.
    virtual srs_error_t parse(srs_internal::SrsConfigBuffer* buffer, SrsConfig* conf = NULL);
    // Marshal the directive to writer.
    // @param level, the root is level0, all its directives are level1, and so on.
    // virtual srs_error_t persistence(SrsFileWriter* writer, int level);
    // // Dumps the args[0-N] to array(string).
    // virtual SrsJsonArray* dumps_args();
    // // Dumps arg0 to str, number or boolean.
    // virtual SrsJsonAny* dumps_arg0_to_str();
    // virtual SrsJsonAny* dumps_arg0_to_integer();
    // virtual SrsJsonAny* dumps_arg0_to_number();
    // virtual SrsJsonAny* dumps_arg0_to_boolean();
public:
    // The directive parsing context.
    enum SrsDirectiveContext {
        // The root directives, parsing file.
        SrsDirectiveContextFile,
        // For each directive, parsing text block.
        SrsDirectiveContextBlock,
    };
    enum SrsDirectiveState {
        // Init state
        SrsDirectiveStateInit,
        // The directive terminated by ';' found
        SrsDirectiveStateEntire,
        // The token terminated by '{' found
        SrsDirectiveStateBlockStart,
        // The '}' found
        SrsDirectiveStateBlockEnd,
        // The config file is done
        SrsDirectiveStateEOF,
    };
    // Parse the conf from buffer. the work flow:
    // 1. read a token(directive args and a ret flag),
    // 2. initialize the directive by args, args[0] is name, args[1-N] is args of directive,
    // 3. if ret flag indicates there are child-directives, read_conf(directive, block) recursively.
    virtual srs_error_t parse_conf(srs_internal::SrsConfigBuffer* buffer, SrsDirectiveContext ctx, SrsConfig* conf);    
    // Read a token from buffer.
    // A token, is the directive args and a flag indicates whether has child-directives.
    // @param args, the output directive args, the first is the directive name, left is the args.
    // @param line_start, the actual start line of directive.
    // @return, an error code indicates error or has child-directives.
    virtual srs_error_t read_token(srs_internal::SrsConfigBuffer* buffer, std::vector<std::string>& args, int& line_start, SrsDirectiveState& state);
};

class SrsConfig
{
    friend class SrsConfDirective;
private:
    bool dolphin;
    std::string dolphin_rtmp_port;
    std::string dolphin_http_port;    
    // Whether show help and exit.
    bool show_help;
     // Whether show SRS version and exit.
    bool show_version;
    // Whether show SRS signature and exit.
    bool show_signature;
 
    // The user parameters, the argc and argv.
    // The argv is " ".join(argv), where argv is from main(argc, argv).

    std::string _argv;
private:
    // The last parsed config file.
    // If  reload, reload the config file.
    std::string config_file;  
protected:
    // The directive root.
    SrsConfDirective* root;  
    // The reload subscribers, when reload, callback all handlers.
    std::vector<ISrsReloadHandler*> subscribes;
public:
    SrsConfig();
    virtual ~SrsConfig();

public:
    // For reload handler to register itself,
    // when config service do the reload, callback the handler.
    virtual void subscribe(ISrsReloadHandler* handler);
    // For reload handler to unregister itself.
    virtual void unsubscribe(ISrsReloadHandler* handler);
    // Parse options and file
public:
    // Parse the cli, the main(argc,argv) function.
    virtual srs_error_t parse_options(int argc, char** argv);
private:
    // Parse each argv.
    virtual srs_error_t parse_argv(int& i, char** argv);    
    // Print help and exit.
    virtual void print_help(char** argv);
public:
    // Parse the config file, which is specified by cli.
    virtual srs_error_t parse_file(const char* filename);
private:    
    // Build a buffer from a src, which is string content or filename.
    virtual srs_error_t build_buffer(std::string src, srs_internal::SrsConfigBuffer** pbuffer);
protected:
    protected:
    // Parse config from the buffer.
    // @param buffer, the config buffer, user must delete it.
    // @remark, use protected for the utest to override with mock.
    virtual srs_error_t parse_buffer(srs_internal::SrsConfigBuffer* buffer);
public:
    // Get the config file path.
    // virtual std::string config();

public:
    // Get the pid file path.
    // The pid file is used to save the pid of SRS,
    // use file lock to prevent multiple SRS starting.
    // @remark, if user need to run multiple SRS instance,
    //       for example, to start multiple SRS for multiple CPUs,
    //       user can use different pid file for each process.
    virtual std::string get_pid_file();
    // Whether log to file.
    virtual bool get_log_tank_file();
    // Get the log level.
    virtual std::string get_log_level();
    // Get the log file path.
    virtual std::string get_log_file();
    // Whether use utc-time to format the time.
    virtual bool get_utc_time();
    // Whether use asprocess mode.
    virtual bool get_asprocess();
    // Whether empty client IP is ok.
    virtual bool empty_ip_ok();
public:
    virtual srs_utime_t get_threads_interval();
// http api section
private:
    // Whether http api enabled
    virtual bool get_http_api_enabled(SrsConfDirective* conf);
public:
    // Whether http api enabled.
    virtual bool get_http_api_enabled();
    // Get the http api listen port.
    virtual std::string get_http_api_listen();
// https api section
private:
    SrsConfDirective* get_https_stream();
public:
    virtual std::string get_https_stream_ssl_key();
    virtual std::string get_https_stream_ssl_cert();
public:
    virtual std::string get_https_api_listen();
public:
    //proxy related conf  
    virtual bool get_http_proxy_enabled();
    virtual std::string get_http_proxy_listen();
    virtual bool get_next_hip_proxy_enabled();
    virtual std::string get_next_hip_proxy_port();
    virtual std::string get_next_hip_proxy_ip();
private:
    SrsConfDirective* get_https_api();
public:
    virtual bool get_https_api_enabled();

// http stream section
private:
    // Whether http stream enabled.
    virtual bool get_http_stream_enabled(SrsConfDirective* conf);
public:
    // Whether http stream enabled.
    // TODO: FIXME: rename to http_static.
    virtual bool get_http_stream_enabled();
    // Get the http stream listen port.
    virtual std::string get_http_stream_listen();
    // Whether enable crossdomain for http static and stream server.
    virtual bool get_http_stream_crossdomain();

public:
    virtual std::string get_https_stream_listen();

};

#endif