#pragma once

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
#include <mutex>
#include <complex>
#include <algorithm>
#include <random>

class ImUsrpUiRx
{
public:
	ImUsrpUiRx(uhd::rx_streamer::sptr stream);
	~ImUsrpUiRx();

    // You must specify a move constructor in order to have a vector of these windows
    // since they contain a child std::thread instance
    ImUsrpUiRx(ImUsrpUiRx&&) = default;

    //// Disable copy ctor
    //ImUsrpUiRx(const ImUsrpUiRx&) = delete;

	void render();

    void thread_process_for_plots();

private:
	uhd::rx_streamer::sptr rx_stream;

    // Separate threads for seamless receives
    std::thread thd; // for receiving alone
    std::thread procthd; // for processing to make data for plots

    // Some other settings, eventually should be in the UI
    std::vector<size_t> channel_nums = { 0 };
    size_t samps_per_buff = 10000; // DEFAULT FOR NOW
    unsigned long long num_requested_samples = 0; // DEFAULT FOR NOW

	// For now, copy the old rx function
    double rxtime[2];
    std::vector<std::complex<short>> buffers[2];
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
    bool stop_signal_called = true;

    // Containers for plotting
    std::vector<std::complex<double>> reimplotdata;
    std::unique_ptr<std::mutex> mtx[3];

    // Simulator function for testing without usrp
    void sim_to_buffer();
};