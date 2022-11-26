#include <srs_app_reload.hpp>
#include <srs_kernel_error.hpp>

ISrsReloadHandler::ISrsReloadHandler()
{
}

ISrsReloadHandler::~ISrsReloadHandler()
{
}

srs_error_t ISrsReloadHandler::on_reload_listen()
{
    return srs_success;
}