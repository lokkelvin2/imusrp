#include "ImUsrpUi.h"

void ImUsrpUi::render()
{
	ImGui::Begin(m_windowname);

	// DEBUGGING
	//printf("FROM RENDER -> %d, Addr: %p\n", *to_make_usrp, to_make_usrp);

	// Widgets go here
	ImGui::InputText("Device Address", device_addr_string, 64);
	if (ImGui::Button("Connect to USRP"))
	{
		// thd_joined = false;
		// thd = std::thread(&ImUsrpUi::usrp_make, this, std::string(device_addr_string));

		//// Signal main thread to make?
		//printf("before signalling, %d\n", *to_make_usrp);
		*to_make_usrp = 1; //  true;
		//printf("signalled main thread? %d\n", *to_make_usrp);

		ImGui::OpenPopup("Initialising USRP..");

		//// Freezes the GUI
		//usrp_make(std::string(device_addr_string));
	}

	if (ImGui::BeginPopupModal("Initialising USRP..", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// // Deprecated
		// if (usrp_ready)
		// {
		// 	if (!thd_joined) {
		// 		thd.join();
		// 		thd_joined = true;
		// 	}
		// 	ImGui::Text("USRP is connected.");
		// 	if (ImGui::Button("Ok", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		// }
		// else
		// {
		// 	ImGui::Text("Please wait while the USRP is being initialised..");
		// }
		// // =========

		// New code where usrp is constructed in main thread
		if (*to_make_usrp == 1)
		{
			ImGui::Text("Please wait while the USRP is being initialised..");
		}
		else
		{
			ImGui::Text("USRP is connected.");
			if (ImGui::Button("Ok", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		}

		ImGui::EndPopup();
	}

	// If the usrp is connected, display some info
	if (usrp_ready)
	{
		render_usrp_info();
		render_subdev_info();
		render_rx_options();
	}



	ImGui::End();
}

void ImUsrpUi::usrp_make(std::string device_addr_string)
{
	usrp = uhd::usrp::multi_usrp::make(device_addr_string);
	//// Lock clocks (TODO: allow ref changes)
	//usrp->set_clock_source("internal");
	//usrp->set_time_source("internal");

	usrp_initialinfo();

	// Flag it
	usrp_ready = true;

	// while (keep_make_thd_alive)
	// {

	// }
}

void ImUsrpUi::usrp_initialinfo()
{
	// Collect initial information
	usrp_pp_string = usrp->get_pp_string();
	tx_subdev_spec = usrp->get_tx_subdev_spec();
	rx_subdev_spec = usrp->get_rx_subdev_spec();
	// Initial ranges
	auto rxrate_range = usrp->get_rx_rates();
	rxratemin = rxrate_range.start();
	rxratemax = rxrate_range.stop();
	rxratestep = rxrate_range.step();

	auto rxfreq_range = usrp->get_rx_freq_range();
	rxfreqmin = rxfreq_range.start();
	rxfreqmax = rxfreq_range.stop();
	rxfreqstep = rxfreq_range.step();

	auto rxgain_range = usrp->get_rx_gain_range();
	rxgainmin = rxgain_range.start();
	rxgainmax = rxgain_range.stop();
	rxgainstep = rxgain_range.step();
}

void ImUsrpUi::render_usrp_info()
{
	if (ImGui::Button("Refresh USRP Info")) { usrp_pp_string = usrp->get_pp_string(); }
	ImGui::Text("%s", usrp_pp_string.c_str());
}

void ImUsrpUi::render_subdev_info()
{
	if (ImGui::Button("Refresh Subdevice Specs")) {
		tx_subdev_spec = usrp->get_tx_subdev_spec();
		rx_subdev_spec = usrp->get_rx_subdev_spec();
	}
	ImGui::Text("TX Details");
	ImGui::Text("%s", tx_subdev_spec.to_pp_string().c_str());
	ImGui::Text("RX Details");
	ImGui::Text("%s", rx_subdev_spec.to_pp_string().c_str());


}

void ImUsrpUi::render_rx_options()
{
	// Display the RX channels for just channel 0
	const size_t chnl = 0;
	ImGui::Text("\nChannel %zd Settings", chnl);

	// Show settings
	ImGui::Text("Ctrl-Click to type directly");
	ImGui::SliderInt("RX Sample Rate", &rxrate, (int)rxratemin, (int)rxratemax);
	ImGui::SliderFloat("RX Centre Frequency (Hz)", &rxfreq, (float)rxfreqmin, (float)rxfreqmax);
	ImGui::SliderFloat("RX Gain (dB)", &rxgain, (float)rxgainmin, (float)rxgainmax);

	if (ImGui::Button("Apply Settings"))
	{
		thd_joined = false;
		waiting_rx_settings = true;
		thd = std::thread(&ImUsrpUi::set_rx_options, this, chnl);

		ImGui::OpenPopup("Configuring RX Channel Settings");
	}
	if (ImGui::BeginPopupModal("Configuring RX Channel Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (waiting_rx_settings) {
			ImGui::Text("Please wait while the RX channel settings are configured..");
		}
		else {
			if (!thd_joined) {
				thd.join();
				thd_joined = true;
			}

			ImGui::Text("RX Channel settings have finished configurations.");
			if (ImGui::Button("Ok")) { ImGui::CloseCurrentPopup(); }
		}

		ImGui::EndPopup();
	}

	// Present the actual values
	ImGui::Text("Actual RX Sample Rate: %f", actualrxrate);
	ImGui::Text("Actual RX Centre Frequency (Hz): %f", actualrxfreq);
	ImGui::Text("Actual RX Gain (dB): %f", actualrxgain);


	if (ImGui::Button("RX"))
	{
		// Modal for stream args?
		ImGui::OpenPopup("Initialise RX stream");
	}

	if (ImGui::BeginPopupModal("Initialise RX stream", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Combo("Wire format", &wirefmtidx, wirefmts,  IM_ARRAYSIZE(wirefmts));
		ImGui::Combo("CPU format", &cpufmtidx, cpufmts,  IM_ARRAYSIZE(cpufmts));

		ImGui::Text("For now, we will only be using channel 0. Configurations come later..");
		ImGui::Text("%s", stream_err);
		if (ImGui::Button("Open RX Stream"))
		{
			// Construct whatever format was selected
			uhd::stream_args_t stream_args(
				cpufmts[cpufmtidx],
				wirefmts[wirefmtidx]
			);
			// For now, restrict to single channel
			std::vector<size_t> channel_nums = { 0 };
			stream_args.channels = channel_nums;
			try
			{
				uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

				rxwindows.emplace_back(
					rx_stream,
					stream_args,
					&actualrxrate, &actualrxfreq, &actualrxgain);

				// Close the modal
				ImGui::CloseCurrentPopup();
			}
			catch (std::runtime_error e)
			{
				snprintf(stream_err, 256, "%s", e.what());
			}

		}
		// Close the modal regardless
		if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }

		ImGui::EndPopup();
	}

	if (rxwindows.size() > 0)
	{
		for (size_t i = 0; i < rxwindows.size(); i++)
		{
			rxwindows.at(i).render();
		}
	}
}

void ImUsrpUi::set_rx_options(size_t chnl)
{
	usrp->set_rx_rate((double)rxrate, chnl);
	actualrxrate = usrp->get_rx_rate(chnl);

	usrp->set_rx_freq(
		uhd::tune_request_t((double)rxfreq),
		chnl
	);
	actualrxfreq = usrp->get_rx_freq(chnl);

	usrp->set_rx_gain((double)rxgain, chnl);
	actualrxgain = usrp->get_rx_gain(chnl);

	// Flag that its done
	waiting_rx_settings = false;
}


ImUsrpUi::ImUsrpUi(volatile int *make_usrp_flag)
	: to_make_usrp{ make_usrp_flag }
{

}

ImUsrpUi::~ImUsrpUi()
{

}

