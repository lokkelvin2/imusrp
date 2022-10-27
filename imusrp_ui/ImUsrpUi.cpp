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
	ImGui::Text(usrp_pp_string.c_str());
}

void ImUsrpUi::render_subdev_info()
{
	if (ImGui::Button("Refresh Subdevice Specs")) {
		tx_subdev_spec = usrp->get_tx_subdev_spec();
		rx_subdev_spec = usrp->get_rx_subdev_spec();
	}
	ImGui::Text("TX Details");
	ImGui::Text(tx_subdev_spec.to_pp_string().c_str());
	ImGui::Text("RX Details");
	ImGui::Text(rx_subdev_spec.to_pp_string().c_str());
}


ImUsrpUi::ImUsrpUi()
{

}

ImUsrpUi::~ImUsrpUi()
{

}

