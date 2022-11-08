#include "ImUsrpUiRx.h"
#include <iostream>

void ImUsrpUiRx::render()
{
	ImGui::Begin("RX Stream");

    // Display the arguments used to construct the stream
    ImGui::Text("Sample rate: %f", m_rxrate==nullptr ? -1.0 : *m_rxrate);
    ImGui::Text("Centre Frequency: %g", m_rxfreq==nullptr ? -1.0 : *m_rxfreq);
    ImGui::Text("Gain: %f", m_rxgain==nullptr ? -1.0 : *m_rxgain);
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
        ImGui::SliderInt("Spectrogram bins", &specgram_bins, 2, 10000);
        if (!checkGoodRadix(specgram_bins)) {
            ImGui::Text("Bad radix FFT size, try changing to a good multiple of small radices..");
        }
        ImGui::Text("Spectrogram frequency resolution = %f Hz/bin", *m_rxrate / specgram_bins);
        // ImGui::Text("Samples per packet from USRP = %zd (fixed for now)", samps_per_buff); // this is technically not the actual packet, but what we set per recv call
        ImGui::Checkbox("Automatically tune spectrogram windows", &auto_specgram_windows);
        if (!auto_specgram_windows)
        {
            ImGui::SliderInt("Number of buffers to use", &num_specgram_buffers_to_use, 1, 100);
        }
        else
        {
            // Fix it here, with a target heatmap density of total 100000 points (note that the resulting number may be slightly more due to clipping, but should be okay..)
            specgram_timepoints = TARGET_SPECGRAM_NUMPTS / specgram_bins;
            int samples_per_timepoint = numHistorySecs * static_cast<int>(*m_rxrate) / specgram_timepoints;
            num_specgram_buffers_to_use = samples_per_timepoint / specgram_bins;
            num_specgram_buffers_to_use = num_specgram_buffers_to_use < 1 ? 1 : num_specgram_buffers_to_use; // ensure at least 1
            ImGui::Text("Using %d buffers (each of %d samples) per spectrogram time point (to maintain high FPS)", num_specgram_buffers_to_use, specgram_bins);
        }
        ImGui::Text("Spectrogram time resolution = %fs", (double)(num_specgram_buffers_to_use * specgram_bins) / *m_rxrate);
        ImGui::Text("Total spectrogram points = %d", specgram_timepoints * specgram_bins);
    }
    else 
    {
        ImGui::Text("History length (seconds): %d", numHistorySecs);
    }

    if (ImGui::Button("Start")) {
        // Calculate the plot points and downsample rates
        numPlotPts = numHistorySecs * numPlotPtsPerSecond;
        plotdsr = (int)*m_rxrate / numPlotPtsPerSecond;
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
            specgramdata.resize(specgram_bins * specgram_timepoints);
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
            ImPlot::SetupAxisLimits(ImAxis_X1,0,(double)numHistorySecs); //, ImGuiCond_Always);
            ImPlot::SetupAxes("Time (s)", "Amplitude");//, ImPlotAxisFlags_Lock); //, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
            ImPlot::PlotLine("Real",
                (double*)reimplotdata.data() + 0,
                numPlotPts, //reimplotdata.size(),
                (double)numHistorySecs / (double)numPlotPts, // xscale ie step 
                0.0, // xstart ie first point
                0, // flags
                0, // offset (in the x-axis, not the data pointer)
                2 * sizeof(double)); // *plotdsr); // stride
            ImPlot::PlotLine("Imag",
                (double*)reimplotdata.data() + 1,
                numPlotPts, //reimplotdata.size(),
                (double)numHistorySecs / (double)numPlotPts, // xscale ie step 
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
            const double leftFreq = -(double)*m_rxrate / 2;
            ImPlot::SetupAxes("Frequency (Hz)", "Power (dB)"); //, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, leftFreq, -leftFreq); //, ImGuiCond_Always);
            ImPlot::PlotLine("FFT",
                spectrumdata.data(),
                spectrumdata.size(),
                *m_rxrate / (double)spectrumdata.size(), // xscale (steps)
                leftFreq, // xstart (leftmost pt)
                0, // flags
                0, // offset
                sizeof(double) // stride
            );
        }

        ImPlot::EndPlot();
    }

    if (!stop_signal_called)
    {
        ImGui::Text("Double-click to reset the axis limits");

        float processingload = (float)(procthdtime / ((double)samps_per_buff / *m_rxrate));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(processingload, 1-processingload, 0.f, 1.f));
        ImGui::ProgressBar(
            processingload,
            ImVec2(0.0f, 0.0f));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("Processing load");

        ImGui::Text("Processing thread: %fs", procthdtime);
        ImGui::Text("Recv thread: %fs (%zd samples per call)", (double)samps_per_buff / *m_rxrate, samps_per_buff);
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
        //printf("Buf: %d. samps recv'd: %d\n", tIdx, (int)num_rx_samps);

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
    // temporary workspace vectors for spectrogram
    std::vector<std::complex<float>> specworkspace(2 * specgram_bins); // double the space, for input and output
    int specctr = 0;


    // prepare the IPP FFT on the entire incoming buffer (this is for the spectrum)
    int sizeSpec = 0, sizeInit = 0, sizeBuf = 0;
    int fftlen = samps_per_buff;
    ippsDFTGetSize_C_32fc(fftlen, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast, &sizeSpec, &sizeInit, &sizeBuf);
    /* memory allocation */
    IppsDFTSpec_C_32fc *pSpec = (IppsDFTSpec_C_32fc*)ippMalloc(sizeSpec);
    Ipp8u* pBuffer = (Ipp8u*)ippMalloc(sizeBuf);
    Ipp8u* pMemInit = (Ipp8u*)ippMalloc(sizeInit);
    ippsDFTInit_C_32fc(fftlen, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast, pSpec, pMemInit);

    // prepare the IPP FFT for the specgram
    sizeSpec = 0; sizeInit = 0; sizeBuf = 0;
    ippsDFTGetSize_C_32fc(specgram_bins, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast, &sizeSpec, &sizeInit, &sizeBuf);
    IppsDFTSpec_C_32fc* pSpecSpecgram = (IppsDFTSpec_C_32fc*)ippMalloc(sizeSpec);
    Ipp8u* pBufferSpecgram = (Ipp8u*)ippMalloc(sizeBuf);
    Ipp8u* pMemInitSpecgram = (Ipp8u*)ippMalloc(sizeInit);
    ippsDFTInit_C_32fc(specgram_bins, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast, pSpecSpecgram, pMemInitSpecgram);


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

                /* SPECTROGRAM COMPUTATIONS */
                // Every time we get a new buffer, we simply overlap add into the waiting workspace until we hit the required index
                for (int k = 0; k < buffers[i].size(); k += specgram_bins)
                {
                    ippsAdd_32fc_I(
                        (Ipp32fc*)&buffers[i].at(k),
                        (Ipp32fc*)specworkspace.data(),
                        specgram_bins
                    );

                    // Increment the counter
                    specctr = (specctr + 1) % num_specgram_buffers_to_use;

                    // If the counter is 0, it's time to perform the FFT
                    if (specctr == 0)
                    {
                        ippsDFTFwd_CToC_32fc(
                            (Ipp32fc*)specworkspace.data(), // input is first half
                            (Ipp32fc*)&specworkspace.at(specgram_bins), // output is second half
                            pSpecSpecgram,
                            pBufferSpecgram
                        );

                        // Take the power of the data
                        ippsPowerSpectr_32fc(
                            (Ipp32fc*)&specworkspace.at(specgram_bins),
                            (Ipp32f*)specworkspace.data(), // reuse the front of the workspace (this only takes half the memory)
                            specgram_bins
                        );

                        // Push back the specgram plot data
                        std::move(
                            specgramdata.begin() + specgram_bins,
                            specgramdata.end(),
                            specgramdata.begin()
                        );

                        // Copy the output and convert into doubles
                        ippsConvert_32f64f(
                            (Ipp32f*)specworkspace.data(),
                            (Ipp64f*)&specgramdata.at(specgramdata.size()-specgram_bins), // copy to the end
                            specgram_bins
                        );

                        // At the end, re-zero the input workspace
                        ippsZero_32fc((Ipp32fc*)specworkspace.data(), specworkspace.size());
                    }
                }

                // record performance
                procthdtime = timer.stop();
            }
        }
    }

    // clearing up
    ippFree(pSpec);
    ippFree(pBuffer);
    ippFree(pMemInit);

    ippFree(pSpecSpecgram);
    ippFree(pBufferSpecgram);
    ippFree(pMemInitSpecgram);



}

bool ImUsrpUiRx::checkGoodRadix(int n)
{
    const int radix[4] = { 2,3,5,7 }; // we limit to these radix
    int r;

    for (int i = 0; i < 4; i++)
    {
        r = radix[i];
        // divide until no longer divisible
        while(n != 1 && n % r == 0)
        {
            n = n / r;
        }
    }
    if (n == 1) { return true; }
    else { return false; }
}


ImUsrpUiRx::ImUsrpUiRx(uhd::rx_streamer::sptr stream, uhd::stream_args_t stream_args,
    double *rxrate, double *rxfreq, double *rxgain)
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
    // End the streams
    stop_signal_called = true;
    if (thd.joinable()){thd.join();}
    if (procthd.joinable()){procthd.join();}
}