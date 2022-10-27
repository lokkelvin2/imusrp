#pragma once
#include "imgui.h"

#include <string>
#include <vector>
#include <iostream>
#include <thread>

// These are all USRP-related includes
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/static.hpp>
#include <uhd/utils/thread.hpp>
//#include <boost/algorithm/string.hpp>
//#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/math/special_functions/round.hpp>
//#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <csignal>
#include <fstream>

// Child windows
#include "ImUsrpUiRx.h"


class ImUsrpUi
{
public:
	ImUsrpUi();
	~ImUsrpUi();

	/// <summary>
	/// Primary render method. Called in the event loop.
	/// </summary>
	void render();

private:
	const char *m_windowname = "ImUSRP";

	/// <summary>
	/// Render section to display/refresh usrp pp_string.
	/// </summary>
	void render_usrp_info();
	std::string usrp_pp_string;

	/// <summary>
	/// Render section to display subdevice information.
	/// </summary>
	void render_subdev_info();
	uhd::usrp::subdev_spec_t tx_subdev_spec, rx_subdev_spec;

	void render_rx_options();

	// Spare threads
	std::thread thd;
	bool thd_joined = true;

	// USRP object
	char device_addr_string[64];
	uhd::usrp::multi_usrp::sptr usrp;
	

	/// <summary>
	/// Method that wraps the usrp make() with a ready flag, for use in a separate thread.
	/// </summary>
	/// <param name="device_addr_string">Device address string</param>
	void usrp_make(std::string device_addr_string);
	bool usrp_ready = false;


	std::vector<ImUsrpUiRx> rxwindows;
};

