#include "ImUsrpUiRx.h"
#include <iostream>

void ImUsrpUiRx::render()
{
	ImGui::Begin("RX Stream");

    if (ImGui::Button("Start")) {
        reimplotdata.resize(3 * 200000); // default size for now, fixed to about 3 seconds?
        // mark the boolean after resize
        stop_signal_called = false;

        thd = std::thread(&ImUsrpUiRx::recv_to_buffer, this,
            channel_nums,
            samps_per_buff,
            num_requested_samples,
            0.0,
            false,
            false,
            false,
            false,
            false
        );
        procthd = std::thread(&ImUsrpUiRx::thread_process_for_plots, this);
        
        printf("Started the threads\n");
    }
    ImGui::SameLine();
    if (ImGui::Button("Simulate"))
    {
        stop_signal_called = false;

        thd = std::thread(&ImUsrpUiRx::sim_to_buffer, this);
        procthd = std::thread(&ImUsrpUiRx::thread_process_for_plots, this);
        
        printf("Started the threads (simulation)\n");

    }

    if (ImGui::Button("End")) {
        stop_signal_called = true;
        thd.join();
        procthd.join();
    }

	// Create the plot
    const int numPlotPts = 3 * 10000;
    int plotdsr = reimplotdata.size() / numPlotPts; // change divisor to number of points to show?
    plotdsr = plotdsr > 0 ? plotdsr : 1;
    //printf("plotdsr = %d\n", plotdsr);
	if (ImPlot::BeginPlot("##iqplot"))
	{
        // note, stride of PlotLine is very slow, likely need to stride externally
        // only plot lines if its running
        if (!stop_signal_called) {
            ImPlot::PlotLine("Real",
                (double*)reimplotdata.data() + 0,
                numPlotPts, //reimplotdata.size(),
                1.0, // xscale ie step 
                0.0, // xstart ie first point
                0, // flags
                0, // offset (in the x-axis, not the data pointer)
                2 * sizeof(double)); // *plotdsr); // stride
            ImPlot::PlotLine("Imag",
                (double*)reimplotdata.data() + 1,
                numPlotPts, //reimplotdata.size(),
                1.0, // xscale ie step 
                0.0, // xstart ie first point
                0, // flags
                0, // offset (in the x-axis, not the data pointer)
                2 * sizeof(double)); // *plotdsr); // stride
        }
        
		ImPlot::EndPlot();
	}

	ImGui::End();
}

void ImUsrpUiRx::sim_to_buffer()
{
    // First create an instance of an engine.
    std::random_device rnd_device;
    // Specify the engine and distribution.
    std::mt19937 mt {rnd_device()};  // Generates random integers
    std::uniform_int_distribution<short> dist {1, 52};

    // auto gen = [&dist, &mt](){
    //                return dist(mt);
    //            };

    int tIdx = 0;
    int cnt = 0;
    while (!stop_signal_called)
    {
        for (int i = 0; i < buffers[tIdx].size(); i++)
        {
            buffers[tIdx].at(i) = {dist(mt), dist(mt)};
        }
        // std::generate(
        //     std::cbegin((short*)buffers[tIdx].data()), 
        //     std::cend((short*)buffers[tIdx].data()),
        //     gen);

        std::this_thread::sleep_for(std::chrono::duration<double>(0.05));
        printf("Simulated into buffer %d\n", tIdx);
        rxtime[tIdx] = cnt * 0.05;

        cnt++;
        tIdx = (tIdx + 1) % 2;
    }

}

void ImUsrpUiRx::recv_to_buffer(
    std::vector<size_t> channel_nums,
    size_t samps_per_buff,
    unsigned long long num_requested_samples,
    double time_requested,
    bool bw_summary,
    bool stats,
    bool null,
    bool enable_size_map,
    bool verbose)
{
    unsigned long long num_total_samps = 0;

    // Make a simple double buffer system
    buffers[0].resize(samps_per_buff);
    buffers[1].resize(samps_per_buff);
    rxtime[0] = 0; rxtime[1] = 0;

    int tIdx = 0; // used for buffer index, 0 or 1

    bool overflow_message = true;

    // setup streaming
    uhd::stream_cmd_t stream_cmd((num_requested_samples == 0)
        ? uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS
        : uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    stream_cmd.num_samps = size_t(num_requested_samples);
    stream_cmd.stream_now = true; // we stream immediately
    rx_stream->issue_stream_cmd(stream_cmd);

    typedef std::map<size_t, size_t> SizeMap;
    SizeMap mapSizes;
    const auto start_time = std::chrono::steady_clock::now();
    const auto stop_time =
        start_time + std::chrono::milliseconds(int64_t(1000 * time_requested));
    // Track time and samps between updating the BW summary
    auto last_update = start_time;
    unsigned long long last_update_samps = 0;

    // metadata holding
    uhd::rx_metadata_t md;

    // Now create the pointer vectors, one for current, one for waiting
    std::vector<std::complex<short>*> buff_ptrs[2];
    buff_ptrs[0].resize(channel_nums.size());
    buff_ptrs[1].resize(channel_nums.size()); // we will be writing the pointers during the loop, not here

    // Run this loop until either time expired (if a duration was given), until
    // the requested number of samples were collected (if such a number was
    // given), or until Ctrl-C was pressed.
    while (not stop_signal_called
        and (num_requested_samples != num_total_samps or num_requested_samples == 0)
        and (time_requested == 0.0 or std::chrono::steady_clock::now() <= stop_time)) {
        const auto now = std::chrono::steady_clock::now();

        //printf("Reading to buffer %d\n", tIdx);

        // write the vector of pointers before receiving (only need to write the buff_ptrs at tIdx)	
        for (size_t i = 0; i < channel_nums.size(); i++) {
            
            buff_ptrs[tIdx].at(i) = buffers[tIdx].data();
            //printf("Setting pointers for %zd.. %p \n", i, buff_ptrs[tIdx].at(i));
        }

        size_t num_rx_samps = 
            rx_stream->recv(buff_ptrs[tIdx], samps_per_buff, md, 3.0, enable_size_map);

        // read metadata for time
        rxtime[tIdx] = md.time_spec.get_real_secs();

        // standard error checks that were copied
        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
            std::cout << boost::format("Timeout while streaming") << std::endl;
            break;
        }
        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
            if (overflow_message) {
                overflow_message = false;
                std::cout << "Got an overflow indication." << std::endl;
                /*std::cerr
                    << boost::format(
                        "Got an overflow indication. Please consider the following:\n"
                        "  Your write medium must sustain a rate of %fMB/s.\n"
                        "  Dropped samples will not be written to the file.\n"
                        "  Please modify this example for your purposes.\n"
                        "  This message will not appear again.\n")
                    % (usrp->get_rx_rate(channel_nums[0]) * sizeof(std::complex<short>) / 1e6);*/

            }
            continue;
            
        }
        if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
            std::string error = str(boost::format("Receiver error: %s") % md.strerror());
            //            if (continue_on_bad_packet) {
            //                std::cerr << error << std::endl;
            //                continue;
            //            } else
            //                throw std::runtime_error(error);
            break; // again, ensure timing integrity, always break
        }

        if (enable_size_map) {
            SizeMap::iterator it = mapSizes.find(num_rx_samps);
            if (it == mapSizes.end())
                mapSizes[num_rx_samps] = 0;
            mapSizes[num_rx_samps] += 1;
        }

        num_total_samps += num_rx_samps;

        // ============ check buffers
        printf("Buf: %d. samps recv'd: %d\n", tIdx, (int)num_rx_samps);

        // update indices
        tIdx = (tIdx + 1) % 2;


        // ==========================

        if (bw_summary) {
            last_update_samps += num_rx_samps;
            const auto time_since_last_update = now - last_update;
            if (time_since_last_update > std::chrono::seconds(1)) {
                const double time_since_last_update_s =
                    std::chrono::duration<double>(time_since_last_update).count();
                const double rate = double(last_update_samps) / time_since_last_update_s;
                std::cout << "\t" << (rate / 1e6) << " Msps" << std::endl;
                last_update_samps = 0;
                last_update = now;
            }
        }
    }
    const auto actual_stop_time = std::chrono::steady_clock::now();

    stream_cmd.stream_mode = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
    rx_stream->issue_stream_cmd(stream_cmd);


    if (stats) {
        std::cout << std::endl;
        const double actual_duration_seconds =
            std::chrono::duration<float>(actual_stop_time - start_time).count();

        std::cout << boost::format("Received %d samples in %f seconds") % num_total_samps
            % actual_duration_seconds
            << std::endl;
        const double rate = (double)num_total_samps / actual_duration_seconds;
        std::cout << (rate / 1e6) << " Msps" << std::endl;

        if (enable_size_map) {
            std::cout << std::endl;
            std::cout << "Packet size map (bytes: count)" << std::endl;
            for (SizeMap::iterator it = mapSizes.begin(); it != mapSizes.end(); it++)
                std::cout << it->first << ":\t" << it->second << std::endl;
        }
    }
}

void ImUsrpUiRx::thread_process_for_plots()
{
    double time = 0.0;
    while (!stop_signal_called)
    {
        // check both times
        for (int i = 0; i < 2; i++)
        {
            if (rxtime[i] > time)
            {
                // update the time
                time = rxtime[i];
                printf("Thread processed time %f\n", time);

                // move the data back by the buffer length
                std::move(reimplotdata.begin() + buffers[i].size(), reimplotdata.end(), reimplotdata.begin());

                // must use transform since the data types are different
                std::transform(
                    buffers[i].cbegin(),
                    buffers[i].cend(),
                    reimplotdata.end() - buffers[i].size(),
                    [](std::complex<short> sc){
                        // return static_cast<std::complex<float>>(sc); // why doesnt this work
                        return std::complex<double>((double)sc.real(), (double)sc.imag());
                    }
                );

            }
        }
    }
    printf("Side process thread ending\n");

}


ImUsrpUiRx::ImUsrpUiRx(uhd::rx_streamer::sptr stream)
	: rx_stream{ stream }
{
    // Initialise unique ptrs of the mutexes
    // we have to do this because mutexes are not movable/copyable
    for (int i = 0; i < 3; i++)
    {
        mtx[i] = std::make_unique<std::mutex>();
    }

}

ImUsrpUiRx::~ImUsrpUiRx()
{

}