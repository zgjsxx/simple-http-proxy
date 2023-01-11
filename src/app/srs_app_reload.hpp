#ifndef SRS_APP_RELOAD_HPP
#define SRS_APP_RELOAD_HPP

#include <srs_core.hpp>

class ISrsReloadHandler
{
public:
    ISrsReloadHandler();
    virtual ~ISrsReloadHandler();
public:
    virtual srs_error_t on_reload_listen();
};

#endif