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

#include "timer.h"
#include "ipp.h"

class ImUsrpUiRx
{
public:
	ImUsrpUiRx(
        uhd::rx_streamer::sptr stream,
        uhd::stream_args_t stream_args=uhd::stream_args_t(),
        double rxrate=0, double rxfreq=0, double rxgain=0
    );
	~ImUsrpUiRx();

    // You must specify a move constructor in order to have a vector of these windows
    // since they contain a child std::thread instance
    ImUsrpUiRx(ImUsrpUiRx&&) = default;

    //// Disable copy ctor
    //ImUsrpUiRx(const ImUsrpUiRx&) = delete;

	void render();

    void thread_process_for_plots();

private:
    // Constructor requirements
	uhd::rx_streamer::sptr rx_stream;
    uhd::stream_args_t m_stream_args;
    double m_rxrate, m_rxfreq, m_rxgain;

    // Separate threads for seamless receives
    std::thread thd; // for receiving alone
    std::thread procthd; // for processing to make data for plots

    // Performance metrics
    double procthdtime = 0;
    double thdtime = 0;

    // Some other settings, eventually should be in the UI
    int numHistorySecs = 3;
    const int numPlotPtsPerSecond = 10000; // for now, fixed due to fps issues
    int numPlotPts;
    int plotdsr = 1;
    
    size_t samps_per_buff = 10000; // DEFAULT FOR NOW
    unsigned long long num_requested_samples = 0; // DEFAULT FOR NOW

	// For now, copy the old rx function
    double rxtime[2];
    std::vector<std::complex<float>> buffers[2];
    std::vector<std::complex<float>*> buff_ptrs[2];
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
    std::vector<double> ampplotdata;
    std::unique_ptr<std::mutex> mtx[3]; // as of now, don't seem to need the triple buffers?

    // Simulator function for testing without usrp
    void sim_to_buffer();
};