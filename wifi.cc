/*This is code for determining the relation between the amount of Wi-Fi wasted and number of nodes in a network*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/athstats-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"


#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>



NS_LOG_COMPONENT_DEFINE ("WifiSystem");


using namespace ns3;

uint32_t numTx[50] = {0};
uint32_t numRx[50] = {0};

Mac48Address nodeAddr[50]; 

bool g_bRtsCts = false;

uint32_t g_srv_start_time = 1;
uint32_t g_cli_start_time = g_srv_start_time + 1;

uint32_t g_sim_end_time = 10;
uint32_t g_srv_end_time = g_sim_end_time - 1;
uint32_t g_cli_end_time  = g_srv_end_time - 1;


#include "ns3/gnuplot.h"

class GnuPlot2D {

	private:
		std::string file_name;
		std::string plot_title;
		std::string x_label;
		std::string y_label;
		Gnuplot2dDataset dataset;
	
	public:
		GnuPlot2D () { 
			file_name = "";
			plot_title = "";
			x_label = "";
			y_label = "";
			dataset.SetTitle ("");
			
			dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
		}
		
		GnuPlot2D (std::string f_name, std::string pl_title, std::string x_lbl, 
			std::string y_lbl) {
			
			file_name = f_name;
			plot_title = pl_title;
			x_label = x_lbl;
			y_label = y_lbl;
			dataset.SetTitle ("");
			
			dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
		}
		
		void setPlotStyle (Gnuplot2dDataset::Style s) {
			dataset.SetStyle (s);
		}
		
		void addPoint (double x, double y) {
			dataset.Add (x, y);
		}
		
		void plotData () {
	
			/* Instantiate the plot and set its title. */
			std::string file_name_png = file_name + ".png";
			std::string file_name_plt = file_name + ".plt";
			Gnuplot plot(file_name_png);
			plot.SetTitle (plot_title);
			
			/* Make the graphics file, which the plot file will create when it
			 is used with Gnuplot, be a PNG file. */
			plot.SetTerminal ("png");
			
			/*Set the labels for each axis. */
			plot.SetLegend (x_label.c_str(), y_label.c_str());
			
			/* Add the dataset to the plot. */
			plot.AddDataset (dataset);	
			
			/* Open the plot file. */
		    std::ofstream plotFile (file_name_plt.c_str());

		    /* Write the plot file. */
		    plot.GenerateOutput (plotFile);

		    /* Close the plot file. */
		    plotFile.close ();
		    
		    std::string cmd = "gnuplot " + file_name_plt;
		    std::system (cmd.c_str());		    
		}
		
		void displayImage () {
			std::string cmd = "eog " + file_name + ".png &";
		    std::system (cmd.c_str());
		}
			
};

	
void ResetCounters () {
	for (int i = 0; i < 50; i++) {
		numTx[i] = numRx[i] = 0;
	}
}

double_t GetTotalTx (uint16_t n) {
	double_t t = 0.0;
	for (uint16_t i = 0; i < n; i++) {
		t += numTx[i];
	}
	return (t);
}

double_t GetTotalRx (uint16_t n) {
	double_t r = 0.0;
	for (uint16_t i = 0; i < n; i++) {
		r += numRx[i];
	}
	return (r);
}

uint32_t GetNodeIDFromContext (std::string context) 
{
	size_t pos1 = context.find ('/', 1);
	if (pos1 == std::string::npos) {
		return 101;
	}
	
	size_t pos2 = context.find ('/', pos1 + 1);
	if (pos2 == std::string::npos) {
		return 101;
	}
	
	std::string st = context.substr(pos1 + 1, pos2 - pos1);
	uint32_t nodeId = atoi (st.c_str());
	
	return nodeId;
}

void StaPhyTx (std::string context,
               Ptr<const Packet> packet,
               WifiMode mode,
               enum WifiPreamble preamble,
               unsigned char ch)
{
	Time t = Simulator::Now ();
	if (t < NanoSeconds (g_cli_start_time) && t > NanoSeconds (g_cli_end_time)) {
		/* Do not consider management frames */
		return;
	}
		
	WifiMacHeader macHdr;
	packet->PeekHeader(macHdr);
	
	uint32_t nodeId = GetNodeIDFromContext (context);
	if (nodeId > 50) {
		return;
	}
	
	/* In RTS-CTS case, there is no need to track data since we (Bianchi) assume that collisions only happen for RTS frames */
	if (!g_bRtsCts && macHdr.IsData() &&
       macHdr.IsToDs () &&
       packet->GetSize() > 1000)
   {							
		++numTx[nodeId];		
				
	}
	
	if (macHdr.IsRts ()) {
		++numTx[nodeId];		
			
	}	
}

/* Callback to track when a frame reached a station */
void StaPhyRx (std::string context,
               Ptr<const Packet> packet,
               double snr,
               WifiMode mode,
               enum WifiPreamble preamble)
{
	Time t = Simulator::Now ();
	if (t < NanoSeconds (g_cli_start_time) && t > NanoSeconds (g_cli_end_time)) {
		/* Do not consider management frames */
		return;
	}
		
	WifiMacHeader macHdr;
	packet->PeekHeader(macHdr);
	
	uint32_t nodeId = GetNodeIDFromContext (context);
	if (nodeId > 50) {
		return;
	}		
	
	/* Check the frame destination */
	if (nodeAddr[nodeId] == macHdr.GetAddr1() && 
       numTx[nodeId] > 0)
   {		
		/* In RTS-CTS case, there is no need to track data since we (Bianchi) assume that collisions only happen for RTS frames */

		if (!g_bRtsCts && macHdr.IsAck()) {							
			++numRx[nodeId];	
						
		}
		if (macHdr.IsCts()) {							
			++numRx[nodeId];
					
		}
	}
}


double_t wifiModelSimulation (uint32_t nWifi, bool enableRtsCts)
{

	/* Creating Wifi station nodes */

	NodeContainer wifiStaNodes;
	wifiStaNodes.Create (nWifi);

	/* Creating Wifi access point */

	NodeContainer wifiApNode;
	g_bRtsCts = enableRtsCts;
	wifiApNode.Create(1);

	/* Creating Wifi Physical channel */

	WifiHelper wifi = WifiHelper::Default ();
	YansWifiPhyHelper wifiphy = YansWifiPhyHelper::Default ();
	YansWifiChannelHelper wifichannel =
               YansWifiChannelHelper::Default ();

	wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

	wifiphy.Set ("RxGain", DoubleValue (0) );

	/* To use Wireshark for tracing */
	wifiphy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

	wifichannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	wifichannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (-20.0));

	wifiphy.SetChannel (wifichannel.Create ());

	std::string phyChMode ("DsssRate2Mbps");
	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
				"DataMode",StringValue (phyChMode),
			"ControlMode",StringValue (phyChMode));

	/* End of physical layer */
	/* Creat MAC layer for Wifi */


	/* Disabling fragmentation of frames */
	Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("4000"));

	/* Configure the Station Manager to use RtsCts */
	std::string RtsCtsFrames = "4000";

	
	/* Turn ON or OFF RTS/CTS for frames  */
	std::string sRtsCts = "4000";
	if (enableRtsCts)
		sRtsCts = "200";
		
	Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                  StringValue (sRtsCts.c_str()));
	
	
	/* Fixing non-unicast data rate to be the same as that of unicast */
	Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                  StringValue (phyChMode));

	/* Create an SSID for both stations and AP */
	Ssid ssid = Ssid ("wifiSystem");

	/* Setup station wifi MAC. Disable active probing to prevent exra control messages */
	NqosWifiMacHelper stmac = NqosWifiMacHelper::Default ();
	stmac.SetType ("ns3::StaWifiMac",
       			"Ssid", SsidValue (ssid),
       			"ActiveProbing", BooleanValue (false));

	NqosWifiMacHelper apMac = NqosWifiMacHelper::Default ();
	apMac.SetType ("ns3::ApWifiMac",
				"Ssid", SsidValue (ssid));

	/* Installing physical and MAC layer on station and AP device */
	NetDeviceContainer staDevices;
	staDevices = wifi.Install (wifiphy, stmac, wifiStaNodes);

	NetDeviceContainer apDevice;
	apDevice = wifi.Install (wifiphy, apMac, wifiApNode);

	for (uint32_t i = 0; i < nWifi; i++) {
		Ptr<Node> nd = wifiStaNodes.Get(i);
		Ptr<NetDevice> dev = nd->GetDevice(0);
		Ptr<WifiNetDevice> wifi_dev =
            DynamicCast<WifiNetDevice>(dev);
		Ptr<WifiMac> mac = wifi_dev->GetMac();
		
		nodeAddr[i] = mac->GetAddress();		
	}

	/* Placing the station nodes at 5m equidistant from access point */

	MobilityHelper stationMobility;
	Ptr<ListPositionAllocator> stPositionAlloc =
         CreateObject<ListPositionAllocator> ();
	stPositionAlloc->Add (Vector (5.0, 0.0, 0.0));	
	stationMobility.SetPositionAllocator (stPositionAlloc);
	stationMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	stationMobility.Install (wifiStaNodes);
	
	MobilityHelper apMobility;
	Ptr<ListPositionAllocator> apPositionAlloc =
         CreateObject<ListPositionAllocator> ();
	apPositionAlloc->Add (Vector (0.0, 0.0, 0.0));	
	apMobility.SetPositionAllocator (apPositionAlloc);
	apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	apMobility.Install(wifiApNode);
	
	/* Adding Network layer : IP stack and IP addresses */
	
	InternetStackHelper wifistack;

	wifistack.Install (wifiApNode);
	wifistack.Install (wifiStaNodes);


	Ipv4AddressHelper ipv4Address;
	NS_LOG_INFO ("Assigning IP Addresses.");
	ipv4Address.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer ifContainer =
            ipv4Address.Assign (staDevices);
	Ipv4InterfaceContainer ifApContainer =
            ipv4Address.Assign (apDevice);

	ifContainer.Add (ifApContainer);

	Address serverAddress;
	serverAddress = Address (ifContainer.GetAddress (nWifi));

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	/* Setting up server application */
	uint16_t port = 4000;
	
	UdpServerHelper echoServer (port);
	ApplicationContainer serverApps =
            echoServer.Install (wifiApNode.Get(0));
	serverApps.Start (Seconds (g_srv_start_time));
	serverApps.Stop (Seconds (g_srv_end_time));	

	/* Setting up client applications */
	uint32_t packetSize = 1023; /* bytes */
	uint32_t numPackets = 800000000; /* a huge number */
	double interval = 0.002; /* seconds; pump the packets ASAP */
	/* Convert to time object */
	Time interPacketInterval = Seconds (interval);
	
	/* Since there is a bug which prevents the effect of different seeds 
	if all applications are started at the same time, we install 
	client applications one-by-one. */
	
	for (uint32_t i = 0; i < nWifi; i++) {

	    	UdpClientHelper stClient (serverAddress, port);
	    	stClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
	    	stClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
	    	stClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
	    	ApplicationContainer app = stClient.Install (wifiStaNodes.Get(i));

    	   app.Start (Seconds (g_cli_start_time + (i/10000.0)));    	
    	   serverApps.Stop (Seconds (g_cli_end_time));
	    	serverApps.Add(app);
    	}

	/* Data collection using flow monitor */
	FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

   
        
    for (NetDeviceContainer::Iterator i =
            staDevices.Begin ();
            i != staDevices.End ();
            ++i)
    {
		Ptr <NetDevice> dev  = *i;
		
		std::ostringstream oss;
		oss << "/NodeList/" << dev->GetNode()->GetId() 
          << "/DeviceList/" << dev->GetIfIndex();
		std::string devPath = oss.str();
		
		Config::Connect (devPath
               + "/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/State/Tx",
               MakeCallback (&StaPhyTx));
		Config::Connect (devPath 
               + "/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/State/RxOk", 
               MakeCallback (&StaPhyRx));			
	}
    
    /* Run the simulation */
    Simulator::Stop (Seconds (g_sim_end_time));
    Simulator::Run ();
    
    /* Collect data */
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier 
            = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats 
            = monitor->GetFlowStats ();
    
    double_t totalSent = 0.0;
    double_t totalRcvd = 0.0;
    
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i
              = stats.begin ();
              i != stats.end ();
              ++i)
    {
    	{
    		Ipv4FlowClassifier::FiveTuple t =
               classifier->FindFlow (i->first);
    		std::cout << "Flow " << i->first << " ("
                   << t.sourceAddress     << " -> " 
                   << t.destinationAddress << "):"; 
    		std::cout << " Tx Bytes: " << i->second.txBytes << "\t";
    		std::cout << " Rx Bytes: " << i->second.rxBytes << "\n";

    		/* The number of bytes shown here by "i->second.rxBytes" is 
    		 the number of bytes received from network layer and above
    		 leaving the MAC and PHY header. Therefore packets will be
    		 of size (1023 + 20 (IP header) + 8 (UDP header)) = 1051B. */
    		
    		totalSent += i->second.txBytes;
    		totalRcvd += i->second.rxBytes;
    	}
    }
    
    std::cout << "\nTotal Tx Bytes: " << totalSent 
              << "\tTotal Rx Bytes: " << totalRcvd 
              << std::endl;
    
  	/* Clearing the simulation */
    	Simulator::Destroy ();	
	
   return totalRcvd;
}


int main (int argc, char *argv[])
{
	Time::SetResolution (Time::NS);
	
	uint16_t numRuns = 5;
	uint16_t n_arr[] = {5,10,15,20,25,30};
	
	std::vector<uint16_t> n 
               (n_arr, n_arr + sizeof(n_arr) / sizeof(uint16_t));
	
	double_t totalRcvdBasic = 0;
	

	double_t totalTxBasic = 0;
	double_t totalRxBasic = 0;

	std::ofstream fsBasic, fsRtsCts;
    	fsBasic.open ("Data-Basic.csv", std::fstream::app);
    	fsRtsCts.open ("Data-RtsCts.csv", std::fstream::app);

    	GnuPlot2D plot_tp ("basic-tp", 
                         "Throughput",
                         "Num nodes", 
                         "throughput");

    	GnuPlot2D plot_cl ("basic-cl",
                         "Collisions", 
                         "Num nodes", 
                         "Coliisions");
    
	for (uint16_t k = 0; k < n.size(); k++) 
   {
		totalRcvdBasic = 0;
		
		totalTxBasic = 0;
		totalRxBasic = 0;

		/* Run the experiment with different seeds */
		for (int i = 1; i <= numRuns; i++) 
      {
         /*Random seeds */
			RngSeedManager::SetRun ((i*137)%431);
			
			ResetCounters ();
			
			std::cout << n[k] << " nodes; run: " 
                   << i << "; basic config ... " 
                   << std::endl;
			
			totalRcvdBasic += wifiModelSimulation (n[k], false);
			totalTxBasic += GetTotalTx (n[k]);
			totalRxBasic += GetTotalRx (n[k]);
			
		}
		
		/* Calculate the average saturation throughput
         simulation time = 100 */

		uint32_t totalRunTime = numRuns *
              (g_cli_end_time - g_cli_start_time); 

		double_t avgThruputBasic = 
            (totalRcvdBasic * 8.0)/(totalRunTime * 2e6);
		
		
		double_t avgCollBasic =  1 - totalRxBasic/totalTxBasic;
		
		std::cout << "Basic throughput: " << n[k] 
                << "  " << avgThruputBasic 
                << " Collisions: " << avgCollBasic 
                << std::endl;
		
		fsBasic << n[k] << "," << avgThruputBasic 
              << "," << avgCollBasic << std::endl;
		
		plot_tp.addPoint(n[k], avgThruputBasic);
		plot_cl.addPoint(n[k], avgCollBasic);
	}
	
	fsBasic.close();
	fsRtsCts.close();
	
	plot_tp.plotData();
	plot_cl.plotData();
	
	plot_tp.displayImage ();
	plot_cl.displayImage ();
	return 0;
}
