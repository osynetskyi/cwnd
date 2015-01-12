#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <stdio.h>
#include "ns3/flow-monitor-module.h"
#include <string>
#include <fstream>
#include "ns3/packet-sink.h"

using namespace ns3;

NodeContainer nodes;
PointToPointHelper pointToPoint;
NetDeviceContainer devices;
InternetStackHelper stack;
Ipv4AddressHelper address;
Ipv4InterfaceContainer interfaces;
FlowMonitorHelper flowmon;
Ptr<FlowMonitor> monitor;

class Arbiter : public Application
{
public:
  Arbiter ()
  {
     m_node = NULL;
     m_startTime = Seconds(0.0);
	 iteration = 0;
     //m_stopTime = Seconds(16.0);
  }

  Ptr<Node> GetNode () const
  {
      //NS_LOG_FUNCTION (this);
      return NULL;
  }
  void SetNode (Ptr<Node> node) {
      //NS_LOG_FUNCTION (this);
      m_node = NULL;
  }

private:
	int iteration;
  void StartApplication (void) {
  //Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  nodes.Create (2);

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  devices = pointToPoint.Install (nodes);
  stack.Install (nodes);
  address.SetBase ("10.1.1.0", "255.255.255.0");

  interfaces = address.Assign (devices);
  //x = 1;
	Setup(1200);
  }

void Setup(int maxbytes) {
	uint16_t port = 9;  // well-known echo port number
  std::cout << "Sending " << maxbytes << " bytes" << std::endl;

  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (maxbytes));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (11.0));

PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (11.0));
	iteration++;
	monitor = flowmon.InstallAll();
	Simulator::Schedule (Seconds(12.0), &Arbiter::getStats, this);
}

void loop(std::map<FlowId, FlowMonitor::FlowStats> stats)
{
	if (stats[2*iteration -1].rxBytes >= 1200) {
		printf("lol!\n");
		Setup(2000);
	} else {
		printf("pwn!\n");
		Setup(1000);
	}
}

void getStats()
{
    monitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
	printf("Rx: %lld\n", stats[2*iteration - 1].rxBytes);
	//printf("Bytes: %lld\n", stats[1].rxBytes);
	loop(stats);
}

};

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int
main (int argc, char *argv[])
{
    //init();
    
	Arbiter arb1;
	arb1.Initialize();

    //apps();

  Simulator::Stop(Seconds(50.0));
  Simulator::Run ();
  Simulator::Destroy ();
	//printf("res %d\n", nodes.GetN());
  return 0;
}
