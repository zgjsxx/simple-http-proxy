#include <algorithm>

#include <srs_app_config.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_file.hpp>
#include <srs_kernel_log.hpp>
#include <srs_kernel_consts.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_core_platform.hpp>
#include <srs_core_auto_free.hpp>
#include <srs_core.hpp>

// when user config an invalid value, macros to perfer true or false.
#define SRS_CONF_PERFER_FALSE(conf_arg) conf_arg == "on"
#define SRS_CONF_PERFER_TRUE(conf_arg) conf_arg != "off"

// Overwrite the config by env.
#define SRS_OVERWRITE_BY_ENV_STRING(key) if (getenv(key)) return getenv(key)
#define SRS_OVERWRITE_BY_ENV_BOOL(key) if (getenv(key)) return SRS_CONF_PERFER_FALSE(string(getenv(key)))
#define SRS_OVERWRITE_BY_ENV_BOOL2(key) if (getenv(key)) return SRS_CONF_PERFER_TRUE(string(getenv(key)))

// default config file.
#define SRS_CONF_DEFAULT_COFNIG_FILE SRS_DEFAULT_CONFIG

const char* _srs_version = "XCORE-" RTMP_SIG_SRS_SERVER;

// '\n'
#define SRS_LF (char)SRS_CONSTS_LF

// '\r'
#define SRS_CR (char)SRS_CONSTS_CR

using namespace srs_internal;
using std::string;
using std::vector;

/**
 * whether the ch is common space.
 */
bool is_common_space(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == SRS_CR || ch == SRS_LF);
}

string srs_config_bool2switch(string sbool)
{
    return sbool == "true"? "on":"off";
}

void set_config_directive(SrsConfDirective* parent, string dir, string value)
{
    SrsConfDirective* d = parent->get_or_create(dir);
    d->name = dir;
    d->args.clear();
    d->args.push_back(value);
}

void SrsConfDirective::remove(SrsConfDirective* v)
{
    std::vector<SrsConfDirective*>::iterator it;
    if ((it = std::find(directives.begin(), directives.end(), v)) != directives.end()) {
        directives.erase(it);
    }
}

srs_error_t srs_config_transform_vhost(SrsConfDirective* root)
{
    srs_error_t err = srs_success;
    
    for (int i = 0; i < (int)root->directives.size(); i++) {
        SrsConfDirective* dir = root->directives.at(i);
        
        // SRS2.0, rename global http_stream to http_server.
        //  SRS1:
        //      http_stream {}
        //  SRS2+:
        //      http_server {}
        if (dir->name == "http_stream") {
            dir->name = "http_server";
            continue;
        }

        // SRS4.0, removed the support of configs:
        //      rtc_server { perf_stat; queue_length; }
        if (dir->name == "rtc_server") {
            std::vector<SrsConfDirective*>::iterator it;
            for (it = dir->directives.begin(); it != dir->directives.end();) {
                SrsConfDirective* conf = *it;

                if (conf->name == "perf_stat" || conf->name == "queue_length") {
                    dir->directives.erase(it);
                    srs_freep(conf);
                    continue;
                }

                ++it;
            }
        }
        
        if (!dir->is_vhost()) {
            continue;
        }
        
        // for each directive of vhost.
        std::vector<SrsConfDirective*>::iterator it;
        for (it = dir->directives.begin(); it != dir->directives.end();) {
            SrsConfDirective* conf = *it;
            string n = conf->name;
            
            // SRS2.0, rename vhost http to http_static
            //  SRS1:
            //      vhost { http {} }
            //  SRS2+:
            //      vhost { http_static {} }
            if (n == "http") {
                conf->name = "http_static";
                srs_warn("transform: vhost.http => vhost.http_static for %s", dir->name.c_str());
                ++it;
                continue;
            }
            
            // SRS3.0, ignore hstrs, always on.
            // SRS1/2:
            //      vhost { http_remux { hstrs; } }
            if (n == "http_remux") {
                SrsConfDirective* hstrs = conf->get("hstrs");
                conf->remove(hstrs);
                srs_freep(hstrs);
            }
            
            // SRS3.0, change the refer style
            //  SRS1/2:
            //      vhost { refer; refer_play; refer_publish; }
            //  SRS3+:
            //      vhost { refer { enabled; all; play; publish; } }
            if ((n == "refer" && conf->directives.empty()) || n == "refer_play" || n == "refer_publish") {
                // remove the old one first, for name duplicated.
                it = dir->directives.erase(it);
                
                SrsConfDirective* refer = dir->get_or_create("refer");
                refer->get_or_create("enabled", "on");
                if (n == "refer") {
                    SrsConfDirective* all = refer->get_or_create("all");
                    all->args = conf->args;
                    srs_warn("transform: vhost.refer to vhost.refer.all for %s", dir->name.c_str());
                } else if (n == "refer_play") {
                    SrsConfDirective* play = refer->get_or_create("play");
                    play->args = conf->args;
                    srs_warn("transform: vhost.refer_play to vhost.refer.play for %s", dir->name.c_str());
                } else if (n == "refer_publish") {
                    SrsConfDirective* publish = refer->get_or_create("publish");
                    publish->args = conf->args;
                    srs_warn("transform: vhost.refer_publish to vhost.refer.publish for %s", dir->name.c_str());
                }
                
                // remove the old directive.
                srs_freep(conf);
                continue;
            }
            
            // SRS3.0, change the mr style
            //  SRS2:
            //      vhost { mr { enabled; latency; } }
            //  SRS3+:
            //      vhost { publish { mr; mr_latency; } }
            if (n == "mr") {
                it = dir->directives.erase(it);
                
                SrsConfDirective* publish = dir->get_or_create("publish");
                
                SrsConfDirective* enabled = conf->get("enabled");
                if (enabled) {
                    SrsConfDirective* mr = publish->get_or_create("mr");
                    mr->args = enabled->args;
                    srs_warn("transform: vhost.mr.enabled to vhost.publish.mr for %s", dir->name.c_str());
                }
                
                SrsConfDirective* latency = conf->get("latency");
                if (latency) {
                    SrsConfDirective* mr_latency = publish->get_or_create("mr_latency");
                    mr_latency->args = latency->args;
                    srs_warn("transform: vhost.mr.latency to vhost.publish.mr_latency for %s", dir->name.c_str());
                }
                
                srs_freep(conf);
                continue;
            }
            
            // SRS3.0, change the publish_1stpkt_timeout
            //  SRS2:
            //      vhost { publish_1stpkt_timeout; }
            //  SRS3+:
            //      vhost { publish { firstpkt_timeout; } }
            if (n == "publish_1stpkt_timeout") {
                it = dir->directives.erase(it);
                
                SrsConfDirective* publish = dir->get_or_create("publish");
                
                SrsConfDirective* firstpkt_timeout = publish->get_or_create("firstpkt_timeout");
                firstpkt_timeout->args = conf->args;
                srs_warn("transform: vhost.publish_1stpkt_timeout to vhost.publish.firstpkt_timeout for %s", dir->name.c_str());
                
                srs_freep(conf);
                continue;
            }
            
            // SRS3.0, change the publish_normal_timeout
            //  SRS2:
            //      vhost { publish_normal_timeout; }
            //  SRS3+:
            //      vhost { publish { normal_timeout; } }
            if (n == "publish_normal_timeout") {
                it = dir->directives.erase(it);
                
                SrsConfDirective* publish = dir->get_or_create("publish");
                
                SrsConfDirective* normal_timeout = publish->get_or_create("normal_timeout");
                normal_timeout->args = conf->args;
                srs_warn("transform: vhost.publish_normal_timeout to vhost.publish.normal_timeout for %s", dir->name.c_str());
                
                srs_freep(conf);
                continue;
            }
            
            // SRS3.0, change the bellow like a shadow:
            //      time_jitter, mix_correct, atc, atc_auto, mw_latency, gop_cache, queue_length
            //  SRS1/2:
            //      vhost { shadow; }
            //  SRS3+:
            //      vhost { play { shadow; } }
            if (n == "time_jitter" || n == "mix_correct" || n == "atc" || n == "atc_auto"
                || n == "mw_latency" || n == "gop_cache" || n == "queue_length" || n == "send_min_interval"
                || n == "reduce_sequence_header") {
                it = dir->directives.erase(it);
                
                SrsConfDirective* play = dir->get_or_create("play");
                SrsConfDirective* shadow = play->get_or_create(conf->name);
                shadow->args = conf->args;
                srs_warn("transform: vhost.%s to vhost.play.%s of %s", n.c_str(), n.c_str(), dir->name.c_str());
                
                srs_freep(conf);
                continue;
            }
            
            // SRS3.0, change the forward.
            //  SRS1/2:
            //      vhost { forward target; }
            //  SRS3+:
            //      vhost { forward { enabled; destination target; } }
            if (n == "forward" && conf->directives.empty() && !conf->args.empty()) {
                conf->get_or_create("enabled")->set_arg0("on");
                
                SrsConfDirective* destination = conf->get_or_create("destination");
                destination->args = conf->args;
                conf->args.clear();
                srs_warn("transform: vhost.forward to vhost.forward.destination for %s", dir->name.c_str());
                
                ++it;
                continue;
            }

            // SRS3.0, change the bellow like a shadow:
            //      mode, origin, token_traverse, vhost, debug_srs_upnode
            //  SRS1/2:
            //      vhost { shadow; }
            //  SRS3+:
            //      vhost { cluster { shadow; } }
            if (n == "mode" || n == "origin" || n == "token_traverse" || n == "vhost" || n == "debug_srs_upnode") {
                it = dir->directives.erase(it);
                
                SrsConfDirective* cluster = dir->get_or_create("cluster");
                SrsConfDirective* shadow = cluster->get_or_create(conf->name);
                shadow->args = conf->args;
                srs_warn("transform: vhost.%s to vhost.cluster.%s of %s", n.c_str(), n.c_str(), dir->name.c_str());
                
                srs_freep(conf);
                continue;
            }

            // SRS4.0, move nack/twcc to rtc:
            //      vhost { nack {enabled; no_copy;} twcc {enabled} }
            // as:
            //      vhost { rtc { nack on; nack_no_copy on; twcc on; } }
            if (n == "nack" || n == "twcc") {
                it = dir->directives.erase(it);
                
                SrsConfDirective* rtc = dir->get_or_create("rtc");
                if (n == "nack") {
                    if (conf->get("enabled")) {
                        rtc->get_or_create("nack")->args = conf->get("enabled")->args;
                    }

                    if (conf->get("no_copy")) {
                        rtc->get_or_create("nack_no_copy")->args = conf->get("no_copy")->args;
                    }
                } else if (n == "twcc") {
                    if (conf->get("enabled")) {
                        rtc->get_or_create("twcc")->args = conf->get("enabled")->args;
                    }
                }
                srs_warn("transform: vhost.%s to vhost.rtc.%s of %s", n.c_str(), n.c_str(), dir->name.c_str());
                
                srs_freep(conf);
                continue;
            }

            // SRS3.0, change the forward.
            //  SRS1/2:
            //      vhost { rtc { aac; } }
            //  SRS3+:
            //      vhost { rtc { rtmp_to_rtc; } }
            if (n == "rtc") {
                SrsConfDirective* aac = conf->get("aac");
                if (aac) {
                    string v = aac->arg0() == "transcode" ? "on" : "off";
                    conf->get_or_create("rtmp_to_rtc")->set_arg0(v);
                    conf->remove(aac); srs_freep(aac);
                    srs_warn("transform: vhost.rtc.aac to vhost.rtc.rtmp_to_rtc %s", v.c_str());
                }

                SrsConfDirective* bframe = conf->get("bframe");
                if (bframe) {
                    string v = bframe->arg0() == "keep" ? "on" : "off";
                    conf->get_or_create("keep_bframe")->set_arg0(v);
                    conf->remove(bframe); srs_freep(bframe);
                    srs_warn("transform: vhost.rtc.bframe to vhost.rtc.keep_bframe %s", v.c_str());
                }

                ++it;
                continue;
            }
            
            ++it;
        }
    }
    
    return err;
}

namespace srs_internal
{
    SrsConfigBuffer::SrsConfigBuffer()
    {
        line = 1;

        pos = last = start = NULL;
        end = start;
    }
    SrsConfigBuffer::~SrsConfigBuffer()
    {
        srs_freepa(start);
    }

    srs_error_t SrsConfigBuffer::fullfill(const char* filename)
    {
        srs_error_t err = srs_success;

        SrsFileReader reader;

        // open file reader.
        if ((err = reader.open(filename)) != srs_success) {
            return srs_error_wrap(err, "open file=%s", filename);
        }
        // read all.
        int filesize = (int)reader.filesize();
        // create buffer
        srs_freepa(start);
        pos = last = start = new char[filesize];
        end = start + filesize;
        // read total content from file.
        ssize_t nread = 0;
        if ((err = reader.read(start, filesize, &nread)) != srs_success) {
            return srs_error_wrap(err, "read %d only %d bytes", filesize, (int)nread);
        }
        
        return err;
    }

    bool SrsConfigBuffer::empty()
    {
        return pos >= end;
    }
}

SrsConfDirective::SrsConfDirective()
{
    conf_line = 0;
}

SrsConfDirective::~SrsConfDirective()
{
    std::vector<SrsConfDirective*>::iterator it;
    for(it = directives.begin(); it != directives.end(); ++it)
    {
        SrsConfDirective* directive = *it;
        srs_freep(directive);
    }
    directives.clear();
}

string SrsConfDirective::arg0()
{
    if (args.size() > 0) {
        return args.at(0);
    }
    
    return "";
}

string SrsConfDirective::arg1()
{
    if (args.size() > 1) {
        return args.at(1);
    }
    
    return "";
}

string SrsConfDirective::arg2()
{
    if (args.size() > 2) {
        return args.at(2);
    }
    
    return "";
}

string SrsConfDirective::arg3()
{
    if (args.size() > 3) {
        return args.at(3);
    }
    
    return "";
}

SrsConfDirective* SrsConfDirective::at(int index)
{
    srs_assert(index < (int)directives.size());
    return directives.at(index);
}

SrsConfDirective* SrsConfDirective::get(string _name)
{
    std::vector<SrsConfDirective*>::iterator it;
    for(it = directives.begin(); it != directives.end(); ++it)
    {
        SrsConfDirective* directive = *it;
        if(directive->name == _name){
            return directive;
        }
    }
    return NULL;
}

SrsConfDirective* SrsConfDirective::get(string _name, string _arg0)
{
    std::vector<SrsConfDirective*>::iterator it;
    for(it = directives.begin(); it != directives.end(); ++it){
        SrsConfDirective* directive = *it;
        if (directive->name == _name && directive->arg0() == _arg0) {
            return directive;
        }        
    }
}

SrsConfDirective* SrsConfDirective::get_or_create(string n)
{
    SrsConfDirective* conf = get(n);
    
    if (!conf) {
        conf = new SrsConfDirective();
        conf->name = n;
        directives.push_back(conf);
    }
    
    return conf;
}

SrsConfDirective* SrsConfDirective::get_or_create(string n, string a0)
{
    SrsConfDirective* conf = get(n, a0);
    
    if (!conf) {
        conf = new SrsConfDirective();
        conf->name = n;
        conf->args.push_back(a0);
        directives.push_back(conf);
    }
    
    return conf;
}

SrsConfDirective* SrsConfDirective::get_or_create(string n, string a0, string a1)
{
    SrsConfDirective* conf = get(n, a0);

    if (!conf) {
        conf = new SrsConfDirective();
        conf->name = n;
        conf->args.push_back(a0);
        conf->args.push_back(a1);
        directives.push_back(conf);
    }

    return conf;
}

SrsConfDirective* SrsConfDirective::set_arg0(string a0)
{
    if (arg0() == a0) {
        return this;
    }
    
    // update a0.
    if (!args.empty()) {
        args.erase(args.begin());
    }
    
    args.insert(args.begin(), a0);
    
    return this;
}

bool SrsConfDirective::is_vhost()
{
    return name == "vhost";
}

bool SrsConfDirective::is_stream_caster()
{
    return name == "stream_caster";
}

srs_error_t SrsConfDirective::parse(SrsConfigBuffer* buffer, SrsConfig* conf)
{
    return parse_conf(buffer, SrsDirectiveContextFile, conf);
}

// see: ngx_conf_parse
srs_error_t SrsConfDirective::parse_conf(SrsConfigBuffer* buffer, SrsDirectiveContext ctx, SrsConfig* conf)
{
    srs_error_t err = srs_success;
    
    while (true) {
        std::vector<string> args;
        int line_start = 0;
        SrsDirectiveState state = SrsDirectiveStateInit;
        if ((err = read_token(buffer, args, line_start, state)) != srs_success) {
            return srs_error_wrap(err, "read token, line=%d, state=%d", line_start, state);
        }

        if (state == SrsDirectiveStateBlockEnd) {
            return ctx == SrsDirectiveContextBlock ? srs_success : srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected \"}\"", buffer->line);
        }
        if (state == SrsDirectiveStateEOF) {
            return ctx != SrsDirectiveContextBlock ? srs_success : srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected end of file, expecting \"}\"", conf_line);
        }
        if (args.empty()) {
            // return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: empty directive", conf_line);
        }
        
        // Build normal directive which is not "include".
        if (args.at(0) != "include") {
            SrsConfDirective* directive = new SrsConfDirective();

            directive->conf_line = line_start;
            directive->name = args[0];
            args.erase(args.begin());
            directive->args.swap(args);

            directives.push_back(directive);

            if (state == SrsDirectiveStateBlockStart) {
                if ((err = directive->parse_conf(buffer, SrsDirectiveContextBlock, conf)) != srs_success) {
                    return srs_error_wrap(err, "parse dir");
                }
            }
            continue;
        }

        // Parse including, allow multiple files.
        vector<string> files(args.begin() + 1, args.end());
        if (files.empty()) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: include is empty directive", buffer->line);
        }
        if (!conf) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: no config", buffer->line);
        }

        for (int i = 0; i < (int)files.size(); i++) {
            std::string file = files.at(i);
            srs_assert(!file.empty());
            // srs_trace("config parse include %s", file.c_str());

            SrsConfigBuffer* include_file_buffer = NULL;
            SrsAutoFree(SrsConfigBuffer, include_file_buffer);
            if ((err = conf->build_buffer(file, &include_file_buffer)) != srs_success) {
                return srs_error_wrap(err, "buffer fullfill %s", file.c_str());
            }

            if ((err = parse_conf(include_file_buffer, SrsDirectiveContextFile, conf)) != srs_success) {
                return srs_error_wrap(err, "parse include buffer");
            }
        }
    }
    
    return err;
}

// see: ngx_conf_read_token
srs_error_t SrsConfDirective::read_token(SrsConfigBuffer* buffer, vector<string>& args, int& line_start, SrsDirectiveState& state)
{
    srs_error_t err = srs_success;
    
    char* pstart = buffer->pos;
    
    bool sharp_comment = false;
    
    bool d_quoted = false;
    bool s_quoted = false;
    
    bool need_space = false;
    bool last_space = true;
    
    while (true) {
        if (buffer->empty()) {
            if (!args.empty() || !last_space) {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID,
                    "line %d: unexpected end of file, expecting ; or \"}\"",
                    buffer->line);
            }
            // srs_trace("config parse complete");

            state = SrsDirectiveStateEOF;
            return err;
        }
        
        char ch = *buffer->pos++;
        
        if (ch == SRS_LF) {
            buffer->line++;
            sharp_comment = false;
        }
        
        if (sharp_comment) {
            continue;
        }
        
        if (need_space) {
            if (is_common_space(ch)) {
                last_space = true;
                need_space = false;
                continue;
            }
            if (ch == ';') {
                state = SrsDirectiveStateEntire;
                return err;
            }
            if (ch == '{') {
                state = SrsDirectiveStateBlockStart;
                return err;
            }
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected '%c'", buffer->line, ch);
        }
        
        // last charecter is space.
        if (last_space) {
            if (is_common_space(ch)) {
                continue;
            }
            pstart = buffer->pos - 1;
            switch (ch) {
                case ';':
                    if (args.size() == 0) {
                        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected ';'", buffer->line);
                    }
                    state = SrsDirectiveStateEntire;
                    return err;
                case '{':
                    if (args.size() == 0) {
                        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected '{'", buffer->line);
                    }
                    state = SrsDirectiveStateBlockStart;
                    return err;
                case '}':
                    if (args.size() != 0) {
                        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected '}'", buffer->line);
                    }
                    state = SrsDirectiveStateBlockEnd;
                    return err;
                case '#':
                    sharp_comment = 1;
                    continue;
                case '"':
                    pstart++;
                    d_quoted = true;
                    last_space = 0;
                    continue;
                case '\'':
                    pstart++;
                    s_quoted = true;
                    last_space = 0;
                    continue;
                default:
                    last_space = 0;
                    continue;
            }
        } else {
            // last charecter is not space
            if (line_start == 0) {
                line_start = buffer->line;
            }
            
            bool found = false;
            if (d_quoted) {
                if (ch == '"') {
                    d_quoted = false;
                    need_space = true;
                    found = true;
                }
            } else if (s_quoted) {
                if (ch == '\'') {
                    s_quoted = false;
                    need_space = true;
                    found = true;
                }
            } else if (is_common_space(ch) || ch == ';' || ch == '{') {
                last_space = true;
                found = 1;
            }
            
            if (found) {
                int len = (int)(buffer->pos - pstart);
                char* aword = new char[len];
                memcpy(aword, pstart, len);
                aword[len - 1] = 0;
                
                string word_str = aword;
                if (!word_str.empty()) {
                    args.push_back(word_str);
                }
                srs_freepa(aword);
                
                if (ch == ';') {
                    state = SrsDirectiveStateEntire;
                    return err;
                }
                if (ch == '{') {
                    state = SrsDirectiveStateBlockStart;
                    return err;
                }
            }
        }
    }
    
    return err;
}


SrsConfig::SrsConfig()
{

}

SrsConfig::~SrsConfig()
{

}

void SrsConfig::subscribe(ISrsReloadHandler* handler)
{
    std::vector<ISrsReloadHandler*>::iterator it;
    it = std::find(subscribes.begin(), subscribes.end(), handler);
    if(it != subscribes.end()) {
        return;
    }

    subscribes.push_back(handler);
}

void SrsConfig::unsubscribe(ISrsReloadHandler* handler)
{
    std::vector<ISrsReloadHandler*>::iterator it;
    
    it = std::find(subscribes.begin(), subscribes.end(), handler);
    if (it == subscribes.end()) {
        return;
    }
    
    subscribes.erase(it);
}


void SrsConfig::print_help(char** argv)
{
    // printf(
    //        "%s, %s, %s, created by %sand %s\n\n"
    //        "Usage: %s <-h?vVgG>|<[-t] -c filename>\n"
    //        "Options:\n"
    //        "   -?, -h              : Show this help and exit 0.\n"
    //        "   -v, -V              : Show version and exit 0.\n"
    //        "   -g, -G              : Show server signature and exit 0.\n"
    //        "   -t                  : Test configuration file, exit with error code(0 for success).\n"
    //        "   -c filename         : Use config file to start server.\n"
    //        "For example:\n"
    //        "   %s -v\n"
    //        "   %s -t -c %s\n"
    //        "   %s -c %s\n",
    //        RTMP_SIG_SRS_SERVER, RTMP_SIG_SRS_URL, RTMP_SIG_SRS_LICENSE,
    //        RTMP_SIG_SRS_AUTHORS, SRS_CONSTRIBUTORS,
    //        argv[0], argv[0], argv[0], SRS_CONF_DEFAULT_COFNIG_FILE,
    //        argv[0], SRS_CONF_DEFAULT_COFNIG_FILE);
}

srs_error_t SrsConfig::parse_options(int argc, char** argv)
{
    srs_error_t err = srs_success;
    
    // argv
    for (int i = 0; i < argc; i++) {
        _argv.append(argv[i]);
        
        if (i < argc - 1) {
            _argv.append(" ");
        }
    }
    
    // Show help if has any argv
    show_help = argc > 1;
    for (int i = 1; i < argc; i++) {
        if ((err = parse_argv(i, argv)) != srs_success) {
            return srs_error_wrap(err, "parse argv");
        }
    }
    
    if (show_help) {
        print_help(argv);
        exit(0);
    }
    
    if (show_version) {
        fprintf(stderr, "%s\n", RTMP_SIG_SRS_VERSION);
        exit(0);
    }
    if (show_signature) {
        fprintf(stderr, "%s\n", RTMP_SIG_SRS_SERVER);
        exit(0);
    }
    
    // first hello message.
    srs_trace(_srs_version);

    // Try config files as bellow:
    //      User specified config(not empty), like user/docker.conf
    //      If user specified *docker.conf, try *srs.conf, like user/srs.conf
    //      Try the default srs config, defined as SRS_CONF_DEFAULT_COFNIG_FILE, like conf/srs.conf
    //      Try config for FHS, like /etc/srs/srs.conf @see https://github.com/ossrs/srs/pull/2711
    if (true) {
        vector<string> try_config_files;
        if (!config_file.empty()) {
            try_config_files.push_back(config_file);
        }
        if (srs_string_ends_with(config_file, "docker.conf")) {
            try_config_files.push_back(srs_string_replace(config_file, "docker.conf", "srs.conf"));
        }
        try_config_files.push_back(SRS_CONF_DEFAULT_COFNIG_FILE);
        try_config_files.push_back("/etc/srs/srs.conf");

        // Match the first exists file.
        string exists_config_file;
        for (int i = 0; i < (int) try_config_files.size(); i++) {
            string try_config_file = try_config_files.at(i);
            if (srs_path_exists(try_config_file)) {
                exists_config_file = try_config_file;
                break;
            }
        }

        if (exists_config_file.empty()) {
            // return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "no config file at %s", srs_join_vector_string(try_config_files, ", ").c_str());
        }

        if (config_file != exists_config_file) {
            srs_warn("user config %s does not exists, use %s instead", config_file.c_str(), exists_config_file.c_str());
            config_file = exists_config_file;
        }
    }

    // Parse the matched config file.
    err = parse_file(config_file.c_str());

    // if (test_conf) {
    //     // the parse_file never check the config,
    //     // we check it when user requires check config file.
    //     if (err == srs_success && (err = srs_config_transform_vhost(root)) == srs_success) {
    //         // if ((err = check_config()) == srs_success) {
    //         //     srs_trace("the config file %s syntax is ok", config_file.c_str());
    //         //     srs_trace("config file %s test is successful", config_file.c_str());
    //         //     exit(0);
    //         // }
    //     }

    //     srs_trace("invalid config%s in %s", srs_error_summary(err).c_str(), config_file.c_str());
    //     srs_trace("config file %s test is failed", config_file.c_str());
    //     exit(srs_error_code(err));
    // }

    if (err != srs_success) {
        return srs_error_wrap(err, "invalid config");
    }

    // transform config to compatible with previous style of config.
    if ((err = srs_config_transform_vhost(root)) != srs_success) {
        return srs_error_wrap(err, "transform");
    }

    ////////////////////////////////////////////////////////////////////////
    // check log name and level
    ////////////////////////////////////////////////////////////////////////
    if (true) {
        std::string log_filename = this->get_log_file();
        if (get_log_tank_file() && log_filename.empty()) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "no log file");
        }
        if (get_log_tank_file()) {
            srs_trace("you can check log by: tail -n 30 -f %s", log_filename.c_str());
            srs_trace("please check SRS by: ./etc/init.d/srs status");
        } else {
            srs_trace("write log to console");
        }
    }
    
    return err;
}


srs_error_t SrsConfig::parse_argv(int& i, char** argv)
{
    srs_error_t err = srs_success;

    char* p = argv[i];

    if(*p++ != '-'){
        show_help = true;
        return err;
    }

    while(*p){
        switch(*p++){
            case '?':
            case 'h':
                show_help = true;
            case 'c':
                show_help = false;
                if (*p) {
                    //srs -c/obj/srs.conf
                    config_file = p;
                    continue;
                }

                //srs -c /obj/srs.conf
                if (argv[++i]) {
                    config_file = argv[i];
                    continue;
                }
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "-c requires params");
            default:
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "invalid option: \"%c\", read help: %s -h",
                    *(p - 1), argv[0]);
        }
    }
    return err;
}

srs_error_t SrsConfig::build_buffer(string src, SrsConfigBuffer** pbuffer)
{
    srs_error_t err = srs_success;

    SrsConfigBuffer* buffer = new SrsConfigBuffer();

    if((err = buffer->fullfill(src.c_str())) != srs_success){
        srs_freep(buffer);
        return srs_error_wrap(err, "read from src %s", src.c_str());
    }
    *pbuffer = buffer;
    return err;
}

srs_error_t SrsConfig::parse_file(const char* filename)
{
    srs_error_t err = srs_success;
    
    config_file = filename;
    
    if (config_file.empty()) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "empty config");
    }

    SrsConfigBuffer* buffer = NULL;
    SrsAutoFree(SrsConfigBuffer, buffer);
    if ((err = build_buffer(config_file, &buffer)) != srs_success) {
        return srs_error_wrap(err, "buffer fullfill %s", config_file.c_str());
    }
    
    if ((err = parse_buffer(buffer)) != srs_success) {
        return srs_error_wrap(err, "parse buffer");
    }
    
    return err;
}

srs_error_t SrsConfig::parse_buffer(SrsConfigBuffer* buffer)
{
    srs_error_t err = srs_success;

    // We use a new root to parse buffer, to allow parse multiple times.
    srs_freep(root);
    root = new SrsConfDirective();

    // Parse root tree from buffer.
    if ((err = root->parse(buffer, this)) != srs_success) {
        return srs_error_wrap(err, "root parse");
    }
    
    // mock by dolphin mode.
    // for the dolphin will start srs with specified params.
    if (dolphin) {
        // for RTMP.
        set_config_directive(root, "listen", dolphin_rtmp_port);
        
        // for HTTP
        set_config_directive(root, "http_server", "");
        SrsConfDirective* http_server = root->get("http_server");
        set_config_directive(http_server, "enabled", "on");
        set_config_directive(http_server, "listen", dolphin_http_port);
        
        // others.
        set_config_directive(root, "daemon", "off");
        set_config_directive(root, "srs_log_tank", "console");
    }
    
    return err;
}

bool SrsConfig::get_log_tank_file()
{
    static bool DEFAULT = true;

    //TODO
    // if (_srs_in_docker) {
    //     DEFAULT = false;
    // }
    
    SrsConfDirective* conf = root->get("srs_log_tank");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0() != "console";
}

string SrsConfig::get_log_level()
{
    static string DEFAULT = "trace";
    
    SrsConfDirective* conf = root->get("srs_log_level");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}


string SrsConfig::get_log_file()
{
    static string DEFAULT = "./objs/srs.log";
    
    SrsConfDirective* conf = root->get("srs_log_file");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}

bool SrsConfig::get_utc_time()
{
    static bool DEFAULT = false;
    
    SrsConfDirective* conf = root->get("utc_time");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return SRS_CONF_PERFER_FALSE(conf->arg0());
}

string SrsConfig::get_pid_file()
{
    static string DEFAULT = "./objs/srs.pid";
    
    SrsConfDirective* conf = root->get("pid");
    
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}

srs_utime_t SrsConfig::get_threads_interval()
{
    static srs_utime_t DEFAULT = 5 * SRS_UTIME_SECONDS;

    SrsConfDirective* conf = root->get("threads");
    if (!conf) {
        return DEFAULT;
    }

    conf = conf->get("interval");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }

    int v = ::atoi(conf->arg0().c_str());
    if (v <= 0) {
        return DEFAULT;
    }

    return v * SRS_UTIME_SECONDS;
}

bool SrsConfig::get_http_api_enabled()
{
    SrsConfDirective* conf = root->get("http_api");
    return get_http_api_enabled(conf);
}

bool SrsConfig::get_http_api_enabled(SrsConfDirective* conf)
{
    SRS_OVERWRITE_BY_ENV_BOOL("SRS_HTTP_API_ENABLED");

    static bool DEFAULT = false;
    
    if (!conf) {
        return DEFAULT;
    }
    
    conf = conf->get("enabled");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return SRS_CONF_PERFER_FALSE(conf->arg0());
}

bool SrsConfig::get_http_stream_enabled()
{
    SrsConfDirective* conf = root->get("http_server");
    return get_http_stream_enabled(conf);
}

bool SrsConfig::get_http_stream_enabled(SrsConfDirective* conf)
{
    SRS_OVERWRITE_BY_ENV_BOOL("SRS_HTTP_SERVER_ENABLED");

    static bool DEFAULT = false;
    
    if (!conf) {
        return DEFAULT;
    }
    
    conf = conf->get("enabled");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return SRS_CONF_PERFER_FALSE(conf->arg0());
}

string SrsConfig::get_http_stream_listen()
{
    SRS_OVERWRITE_BY_ENV_STRING("SRS_HTTP_SERVER_LISTEN");

    static string DEFAULT = "8080";
    
    SrsConfDirective* conf = root->get("http_server");
    if (!conf) {
        return DEFAULT;
    }
    
    conf = conf->get("listen");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}

string SrsConfig::get_https_stream_listen()
{
    SRS_OVERWRITE_BY_ENV_STRING("SRS_HTTP_SERVER_HTTTPS_LISTEN");

    static string DEFAULT = "8088";

    SrsConfDirective* conf = get_https_stream();
    if (!conf) {
        return DEFAULT;
    }

    conf = conf->get("listen");
    if (!conf) {
        return DEFAULT;
    }

    return conf->arg0();
}

string SrsConfig::get_http_api_listen()
{
    SRS_OVERWRITE_BY_ENV_STRING("SRS_HTTP_API_LISTEN");

    static string DEFAULT = "1985";
    
    SrsConfDirective* conf = root->get("http_api");
    
    if (!conf) {
        return DEFAULT;
    }
    
    conf = conf->get("listen");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}

string SrsConfig::get_https_api_listen()
{
    SRS_OVERWRITE_BY_ENV_STRING("SRS_HTTP_API_HTTPS_LISTEN");

#ifdef SRS_UTEST
    // We should not use static default, because we need to reset for different testcase.
    string DEFAULT = "";
#else
    static string DEFAULT = "";
#endif

    // Follow the HTTPS server if config HTTP API as the same of HTTP server.
    if (DEFAULT.empty()) {
        if (get_http_api_listen() == get_http_stream_listen()) {
            DEFAULT = get_https_stream_listen();
        } else {
            DEFAULT = "1990";
        }
    }

    SrsConfDirective* conf = get_https_api();
    if (!conf) {
        return DEFAULT;
    }

    conf = conf->get("listen");
    if (!conf) {
        return DEFAULT;
    }

    return conf->arg0();
}

SrsConfDirective* SrsConfig::get_https_stream()
{
    SrsConfDirective* conf = root->get("http_server");
    if (!conf) {
        return NULL;
    }

    return conf->get("https");
}

bool SrsConfig::get_asprocess()
{
    static bool DEFAULT = false;
    
    SrsConfDirective* conf = root->get("asprocess");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return SRS_CONF_PERFER_FALSE(conf->arg0());
}

bool SrsConfig::empty_ip_ok()
{
    static bool DEFAULT = true;

    SrsConfDirective* conf = root->get("empty_ip_ok");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }

    return SRS_CONF_PERFER_TRUE(conf->arg0());
}

bool SrsConfig::get_http_stream_crossdomain()
{
    static bool DEFAULT = true;
    
    SrsConfDirective* conf = root->get("http_server");
    if (!conf) {
        return DEFAULT;
    }
    
    conf = conf->get("crossdomain");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return SRS_CONF_PERFER_TRUE(conf->arg0());
}

string SrsConfig::get_https_stream_ssl_key()
{
    static string DEFAULT = "./conf/server.key";

    SrsConfDirective* conf = get_https_stream();
    if (!conf) {
        return DEFAULT;
    }

    conf = conf->get("key");
    if (!conf) {
        return DEFAULT;
    }

    return conf->arg0();
}

string SrsConfig::get_https_stream_ssl_cert()
{
    static string DEFAULT = "./conf/server.crt";

    SrsConfDirective* conf = get_https_stream();
    if (!conf) {
        return DEFAULT;
    }

    conf = conf->get("cert");
    if (!conf) {
        return DEFAULT;
    }

    return conf->arg0();
}

SrsConfDirective* SrsConfig::get_https_api()
{
    SrsConfDirective* conf = root->get("http_api");
    if (!conf) {
        return NULL;
    }

    return conf->get("https");
}

bool SrsConfig::get_https_api_enabled()
{
    SRS_OVERWRITE_BY_ENV_BOOL("SRS_HTTP_API_HTTPS_ENABLED");

    static bool DEFAULT = false;

    SrsConfDirective* conf = get_https_api();
    if (!conf) {
        return DEFAULT;
    }

    conf = conf->get("enabled");
    if (!conf) {
        return DEFAULT;
    }

    return SRS_CONF_PERFER_FALSE(conf->arg0());
}