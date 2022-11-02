#include "ImUsrpUiRx.h"
#include <iostream>

void ImUsrpUiRx::render()
{
	ImGui::Begin("RX Stream");

    // Display the arguments used to construct the stream
    ImGui::Text("CPU format: %s", m_stream_args.cpu_format.c_str());
    ImGui::Text("Wire format: %s", m_stream_args.otw_format.c_str());
    if (ImGui::TreeNode("Channels in stream"))
    {
        for (auto chnl : m_stream_args.channels)
        {
            ImGui::BulletText("%zd", chnl);
        }
        ImGui::TreePop();
    }

    // Place all the options here?
    if (stop_signal_called)
    {
        ImGui::SliderInt("History length (seconds)", &numHistorySecs, 1, 10);
    }
    else 
    {
        ImGui::Text("History length (seconds): %d", numHistorySecs);
    }

    if (ImGui::Button("Start")) {
        // Calculate the plot points and downsample rates
        numPlotPts = numHistorySecs * numPlotPtsPerSecond;
        plotdsr = (int)m_rxrate / numPlotPtsPerSecond;
        // We must make sure that this is a divisor of samps_per_buff
        if ((int)samps_per_buff % plotdsr != 0)
        {
            ImGui::OpenPopup("Non-divisor DSR");
        }
        else // if okay, start the recording
        {
            // We resize to the number of plot points only
            reimplotdata.resize(numPlotPts);
            spectrumdata.resize(samps_per_buff);
            // mark the boolean after resize
            stop_signal_called = false;

            thd = std::thread(&ImUsrpUiRx::recv_to_buffer, this,
                m_stream_args.channels, // channel_nums,
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
        }

        if (ImGui::BeginPopupModal("Non-divisor DSR", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Plot DSR is %d, but samples received per call is %zd", plotdsr, samps_per_buff);
            if (ImGui::Button("Ok")) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
    ImGui::SameLine();
    
    /* SIMULATION ONLY. FOR DEBUGGING WITHOUT A USRP */
    if (ImGui::Button("Simulate"))
    {
        reimplotdata.resize(numHistorySecs * 200000);
        stop_signal_called = false;

        thd = std::thread(&ImUsrpUiRx::sim_to_buffer, this);
        procthd = std::thread(&ImUsrpUiRx::thread_process_for_plots, this);
    }
    /* END OF SIMULATION ONLY. */

    if (ImGui::Button("End")) {
        stop_signal_called = true;
        thd.join();
        procthd.join();

        ImGui::OpenPopup("Stream Ended Safely");
    }

    if (ImGui::BeginPopupModal("Stream Ended Safely", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Receiver and processor threads have ended safely.");
        if (ImGui::Button("Ok")) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }

	// Create the plot
    ImGui::Text("Amplitude Plot Downsample Rate: %d", plotdsr);
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

    if (ImPlot::BeginPlot("##fftplot"))
    {
        if (!stop_signal_called)
        {
            ImPlot::PlotLine("FFT",
                spectrumdata.data(),
                spectrumdata.size(),
                1.0,
                0.0,
                0,
                0,
                sizeof(double)
            );
        }

        ImPlot::EndPlot();
    }

     if (!stop_signal_called)
     {
         ImGui::Text("Processing thread: %fs", procthdtime);
         ImGui::Text("Recv thread: %fs (%zd samples per call)", (double)samps_per_buff / m_rxrate, samps_per_buff);
     }

	ImGui::End();
}

void ImUsrpUiRx::sim_to_buffer()
{
    buffers[0].resize(10000);
    buffers[1].resize(10000);

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
        for (size_t i = 0; i < buffers[tIdx].size(); i++)
        {
            buffers[tIdx].at(i) = {(float)dist(mt), (float)dist(mt)};
            // if (i < 5){printf("%hd, %hd\n", buffers[tIdx].at(i).real(), buffers[tIdx].at(i).imag());}
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
    HighResolutionTimer timer;

    int numPerBatch = samps_per_buff / plotdsr;
    // temporary workspace vectors
    std::vector<std::complex<float>> workspace(samps_per_buff);
    std::vector<float> workspacereal(samps_per_buff);

    // prepare the IPP FFT on the entire incoming buffer
    int sizeSpec = 0, sizeInit = 0, sizeBuf = 0;
    int fftlen = samps_per_buff;
    ippsDFTGetSize_C_32fc(fftlen, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast, &sizeSpec, &sizeInit, &sizeBuf);
    /* memory allocation */
    IppsDFTSpec_C_32fc *pSpec = (IppsDFTSpec_C_32fc*)ippMalloc(sizeSpec);
    Ipp8u* pBuffer = (Ipp8u*)ippMalloc(sizeBuf);
    Ipp8u* pMemInit = (Ipp8u*)ippMalloc(sizeInit);
    ippsDFTInit_C_32fc(fftlen, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast, pSpec, pMemInit);

    double time = 0.0;
    while (!stop_signal_called)
    {
        // check both times
        for (int i = 0; i < 2; i++)
        {
            if (rxtime[i] > time)
            {
                timer.start();

                // update the time
                time = rxtime[i];

                /* AMPLITUDE PLOT COMPUTATIONS */
                // move the data back by the buffer length
                std::move(reimplotdata.begin() + numPerBatch, reimplotdata.end(), reimplotdata.begin());

                // we now add new points from the new batch, but downsample immediately
                int dsphase = 0;
                int numStored;
                ippsSampleDown_32fc(
                    (Ipp32fc*)buffers[i].data(),
                    (int)buffers[i].size(),
                    (Ipp32fc*)workspace.data(),
                    &numStored,
                    plotdsr,
                    &dsphase
                );
                if (numStored != numPerBatch) { break; } // sanity check

                // Convert data type to double
                ippsConvert_32f64f(
                    (Ipp32f*)workspace.data(),
                    (Ipp64f*)&reimplotdata.at(reimplotdata.size() - numPerBatch),
                    numPerBatch * 2
                );

                /* SPECTRUM PLOT COMPUTATIONS */
                ippsDFTFwd_CToC_32fc(
                    (Ipp32fc*)buffers[i].data(),
                    (Ipp32fc*)workspace.data(),
                    pSpec,
                    pBuffer
                );

                // first take abs squared
                ippsPowerSpectr_32fc(
                    (Ipp32fc*)workspace.data(),
                    (Ipp32f*)workspacereal.data(),
                    (int)workspace.size()
                );
                // then log10
                ippsLog10_32f_A11(
                    (Ipp32f*)workspacereal.data(),
                    (Ipp32f*)workspace.data(), // we reuse the complex workspace
                    (int)workspace.size()
                );
                // then * 10 for dB
                ippsMulC_32f(
                    (Ipp32f*)workspace.data(),
                    10.0f,
                    (Ipp32f*)workspacereal.data(),
                    (int)workspace.size()
                );

                // fftshift (assume even valued size)
                ippsCopy_32f(
                    (Ipp32f*)workspacereal.data(), 
                    (Ipp32f*)workspace.data(), // again reuse the complex workspace
                    (int)workspace.size()/2
                ); // copy left half out
                ippsCopy_32f(
                    (Ipp32f*)&workspacereal.at(workspace.size()/2),
                    (Ipp32f*)&workspacereal.at(0),
                    (int)workspace.size()/2
                ); // move right half to left
                ippsCopy_32f(
                    (Ipp32f*)workspace.data(),
                    (Ipp32f*)&workspacereal.at(workspace.size()/2),
                    (int)workspace.size()/2
                ); // copy left half to right
                

                // Convert data type to double as well
                ippsConvert_32f64f(
                    (Ipp32f*)workspacereal.data(),
                    (Ipp64f*)spectrumdata.data(),
                    spectrumdata.size()
                );


                // record performance
                procthdtime = timer.stop();
            }
        }
    }

    // clearing up
    ippFree(pSpec);
    ippFree(pBuffer);
    ippFree(pMemInit);

}


ImUsrpUiRx::ImUsrpUiRx(uhd::rx_streamer::sptr stream, uhd::stream_args_t stream_args,
    double rxrate, double rxfreq, double rxgain)
    : rx_stream{ stream }, m_stream_args{ stream_args },
    m_rxrate{ rxrate }, m_rxfreq{ rxfreq }, m_rxgain{ rxgain }
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