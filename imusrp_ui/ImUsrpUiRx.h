#pragma once

// Enable 32-bit indexing for implot
#define IMGUI_USER_CONFIG "imusrp_imgui_config.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include <uhd/exception.hpp>
//#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
//#include <uhd/utils/safe_main.hpp>
//#include <uhd/utils/static.hpp>
//#include <uhd/utils/thread.hpp>

#include <thread>
#include <complex>

class ImUsrpUiRx
{
public:
	ImUsrpUiRx(uhd::rx_streamer::sptr stream);
	~ImUsrpUiRx();

    //ImUsrpUiRx(const ImUsrpUiRx&) = delete;
    //ImUsrpUiRx& operator =(const ImUsrpUiRx&) = delete;

	void render();

private:
	uhd::rx_streamer::sptr rx_stream;

    // Separate threads for seamless receives
    std::thread *thd; // pointer otherwise not copyable, hackish for now

	// For now, copy the old rx function
    void recv_to_buffer(
        std::vector<size_t> channel_nums,
        size_t samps_per_buff,
        unsigned long long num_requested_samples,
        double time_requested = 0.0,
        bool bw_summary = false,
        bool stats = false,
        bool null = false,
        bool enable_size_map = false,
        bool verbose = false);
    bool stop_signal_called = false;
};