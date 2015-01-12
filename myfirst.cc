#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <stdio.h>
#include "ns3/flow-monitor-module.h"

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
     m_startTime = Seconds(0);
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

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  devices = pointToPoint.Install (nodes);
  stack.Install (nodes);
  address.SetBase ("10.1.1.0", "255.255.255.0");

  interfaces = address.Assign (devices);
  //x = 1;
	Setup(4);
  }

void Setup(int packets) {
	printf("Sending %d packets\n", packets);
	UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (11.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (packets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (11.0));
	iteration++;
  monitor = flowmon.InstallAll();
	Simulator::Schedule (Seconds(12.0), &Arbiter::getStats, this);
}

void loop(std::map<FlowId, FlowMonitor::FlowStats> stats)
{
	if (stats[iteration*2].rxPackets == 6) {
		Simulator::Schedule (Seconds(1.0), &Arbiter::Setup, this, 2);
	} else {
		Setup(6);
	}
}

void getStats()
{
    monitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
	printf("Rx: %d\n", stats[iteration*2].rxPackets);
	Simulator::Schedule (Simulator::Now(), &Arbiter::loop, this, stats);
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

  Simulator::Stop(Seconds(100.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
