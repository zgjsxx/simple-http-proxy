#include <srs_app_hourglass.hpp>
#include <srs_kernel_log.hpp>
#include <algorithm>

ISrsFastTimer::ISrsFastTimer()
{
}

ISrsFastTimer::~ISrsFastTimer()
{
}

SrsFastTimer::SrsFastTimer(std::string label, srs_utime_t interval)
{
    interval_ = interval;
    trd_ = new SrsSTCoroutine(label, this, _srs_context->get_id());
}

SrsFastTimer::~SrsFastTimer()
{
    srs_freep(trd_);
}

srs_error_t SrsFastTimer::start()
{
    srs_error_t err = srs_success;

    if ((err = trd_->start()) != srs_success) {
        return srs_error_wrap(err, "start timer");
    }

    return err;
}

void SrsFastTimer::subscribe(ISrsFastTimer* timer)
{
    if (std::find(handlers_.begin(), handlers_.end(), timer) == handlers_.end()) {
        handlers_.push_back(timer);
    }
}

void SrsFastTimer::unsubscribe(ISrsFastTimer* timer)
{
    std::vector<ISrsFastTimer*>::iterator it = std::find(handlers_.begin(), handlers_.end(), timer);
    if (it != handlers_.end()) {
        it = handlers_.erase(it);
    }
}

srs_error_t SrsFastTimer::cycle()
{
    srs_error_t err = srs_success;

    while (true) {
        if ((err = trd_->pull()) != srs_success) {
            return srs_error_wrap(err, "quit");
        }

        // ++_srs_pps_timer->sugar;

        for (int i = 0; i < (int)handlers_.size(); i++) {
            ISrsFastTimer* timer = handlers_.at(i);

            if ((err = timer->on_timer(interval_)) != srs_success) {
                srs_freep(err); // Ignore any error for shared timer.
            }
        }

        srs_usleep(interval_);
    }

    return err;
}