#include "ImUsrpUi.h"

void ImUsrpUi::render()
{
	ImGui::Begin(m_windowname);

	// Widgets go here
	ImGui::InputText("Device Address", device_addr_string, 64);
	if (ImGui::Button("Connect to USRP"))
	{
		thd_joined = false;
		thd = std::thread(&ImUsrpUi::usrp_make, this, std::string(device_addr_string));

		ImGui::OpenPopup("Initialising USRP..");
	}

	if (ImGui::BeginPopupModal("Initialising USRP..", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{

		if (usrp_ready)
		{
			if (!thd_joined) {
				thd.join();
				thd_joined = true;
			}
			ImGui::Text("USRP is connected.");
			if (ImGui::Button("Ok", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		}
		else
		{
			ImGui::Text("Please wait while the USRP is being initialised..");
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
	// Collect initial information
	usrp_pp_string = usrp->get_pp_string();
	tx_subdev_spec = usrp->get_tx_subdev_spec();
	rx_subdev_spec = usrp->get_rx_subdev_spec();
	// Flag it
	usrp_ready = true;
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
	if (ImGui::Button("RX"))
	{
		// DEFAULT RATE ARGUMENTS FOR NOW
		usrp->set_rx_rate(240e3, 0);
		uhd::tune_request_t tune_request(100e6, 0.0);
		usrp->set_rx_freq(tune_request, 0);

		// Modal for stream args?
		ImGui::OpenPopup("Initialise RX stream");
	}

	if (ImGui::BeginPopupModal("Initialise RX stream", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Combo("Wire format", &wirefmtidx, wirefmts,  IM_ARRAYSIZE(wirefmts));
		ImGui::Combo("CPU format", &cpufmtidx, cpufmts,  IM_ARRAYSIZE(cpufmts));

		ImGui::Text("For now, we will only be using channel 0. Configurations come later..");
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
			uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

			rxwindows.emplace_back(rx_stream, stream_args);

			// Close the modal
			ImGui::CloseCurrentPopup();
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


ImUsrpUi::ImUsrpUi()
{

}

ImUsrpUi::~ImUsrpUi()
{

}

