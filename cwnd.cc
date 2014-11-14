#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/double.h"

// simulation time
#define SIMTIME 30

// number of different strategies
#define NUM 3

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CWnd");

// global variable for CWnds
uint32_t cwnds[NUM];

static void CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
	// GetContext is used to find a NodeId of a node that triggered the event
	//int num = Simulator::GetContext () - 1;
	NS_LOG_UNCOND ("Sender 1" << " " << Simulator::Now ().GetSeconds () << "\t" << newCwnd);
	//cwnds[num - 1] = newCwnd;
}

static void RttChange (Time oldRtt, Time newRtt)
{
	int num = Simulator::GetContext () - 1;
	NS_LOG_UNCOND ("Rtt " << num << " " << newRtt.GetSeconds());
	NS_LOG_UNCOND ("Approx. throughput " << num << " " << Simulator::Now ().GetSeconds () << "\t" 
					<< cwnds[num-1] / newRtt.GetSeconds() * 8 / 1024 / 1024);
}

int 
main (int argc, char *argv[])
{
	uint32_t alpha[NUM], beta[NUM];
	int i = 0;
	//char buf_a[7];
	//char buf_b[6];
	/*CommandLine cmd;

	for(i = 0; i < NUM; i++)
	{
		sprintf(buf_a, "alpha_%d", i + 1);
		cmd.AddValue(buf_a, buf_a, alpha[i]);		
		sprintf(buf_b, "beta_%d", i + 1);		
		cmd.AddValue(buf_b, buf_b, beta[i]);
		cwnds[i] = 0;
	}
	cmd.Parse(argc, argv);*/
	alpha[0] = 1000;
	alpha[1] = 2000;
	alpha[2] = 3000;
	beta[0] = 0;
	beta[1] = 0;
	beta[2] = 0;

	NodeContainer p2pNodes, csmaNodesSend, csmaNodesReceive;
	p2pNodes.Create (2);
	csmaNodesSend.Add(p2pNodes.Get (0));
	csmaNodesSend.Create (NUM);
	csmaNodesReceive.Add(p2pNodes.Get (1));
	csmaNodesReceive.Create (NUM);

	// bottleneck link
	PointToPointHelper pointToPoint;
	//pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("7Mbps"));
	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("15Mbps"));
	//pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
	CsmaHelper csmaHelperSend, csmaHelperReceive;
	//csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
	//csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
	//csmaHelper.SetDeviceAttribute ("Mtu", UintegerValue (9000));

	NetDeviceContainer p2pDevices, csmaDevicesSend, csmaDevicesReceive;
	p2pDevices = pointToPoint.Install (p2pNodes);
	csmaDevicesSend = csmaHelperSend.Install (csmaNodesSend);
	csmaDevicesReceive = csmaHelperReceive.Install (csmaNodesReceive);

	InternetStackHelper stack;
	/*InternetStackHelper stack2;
	stack.SetTcp ("ns3::NscTcpL4Protocol",
                            "Library", StringValue ("liblinux2.6.26.so"));*/
	stack.Install (csmaNodesSend);
	stack.Install (csmaNodesReceive);
	/*stack.Install(csmaNodesSend.Get(1));
	stack.Install(csmaNodesSend.Get(2));
	stack.Install(csmaNodesSend.Get(3));
	stack.Install(csmaNodesReceive.Get(1));
	stack.Install(csmaNodesReceive.Get(2));
	stack.Install(csmaNodesReceive.Get(3));

	stack2.Install(p2pNodes);*/

	Ipv4AddressHelper address;
	address.SetBase ("2.2.2.0", "255.255.255.0");
	Ipv4InterfaceContainer p2pInterfaces;
	p2pInterfaces = address.Assign (p2pDevices);
	address.SetBase ("1.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer csmaInterfacesSend;
	csmaInterfacesSend = address.Assign (csmaDevicesSend);
	address.SetBase ("3.3.3.0", "255.255.255.0");
	Ipv4InterfaceContainer csmaInterfacesReceive;
	csmaInterfacesReceive = address.Assign (csmaDevicesReceive);

	// enable routing
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	uint16_t sinkPort = 8080;
	Address sinkAddress[NUM];
	ApplicationContainer sinkApps[NUM];
	PacketSinkHelper *packetSinkHelper[NUM];

	TypeId tid = TypeId::LookupByName ("ns3::TcpNewReno");
	std::stringstream nodeId[NUM];
	std::string specificNode[NUM];
	Ptr<Socket> ns3TcpSocket[NUM];

	//Ptr<MyApp> app[NUM];
	//Ptr<PacketSink> sink[NUM];

	for(i = 0; i < NUM; i++)
	{
		// create packet sinks on receiving nodes
		sinkAddress[i] = InetSocketAddress (csmaInterfacesReceive.GetAddress (i + 1), sinkPort);
		packetSinkHelper[i] = new PacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
		sinkApps[i] = packetSinkHelper[i]->Install (csmaNodesReceive.Get (i + 1));
		sinkApps[i].Start (Seconds (0.));
		sinkApps[i].Stop (Seconds (SIMTIME));
	
		// open sockets on senders and set their attributes
		nodeId[i] << csmaNodesSend.Get (i + 1)->GetId ();
		specificNode[i] = "/NodeList/" + nodeId[i].str () + "/$ns3::TcpL4Protocol/SocketType";
		Config::Set (specificNode[i], TypeIdValue (tid));
		ns3TcpSocket[i] = Socket::CreateSocket (csmaNodesSend.Get (i + 1), TcpSocketFactory::GetTypeId ());
		ns3TcpSocket[i]->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
		//ns3TcpSocket[i]->TraceConnectWithoutContext ("RWND", MakeCallback (&RwndChange));
		ns3TcpSocket[i]->TraceConnectWithoutContext ("RTT", MakeCallback (&RttChange));
		ns3TcpSocket[i]->SetAttribute("SegmentSize", UintegerValue(alpha[i]));
		ns3TcpSocket[i]->SetAttribute("SlowStartThreshold", UintegerValue(beta[i]));
	}

		/*// setup the data-sending apps on sender nodes
		app[i] = CreateObject<MyApp> ();
		//app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 100000, DataRate ("5Mbps"));
		app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 10000, DataRate ("5Mbps"));
		csmaNodesSend.Get (i + 1)->AddApplication (app[i]);
		app[i]->SetStartTime (Seconds (0.));
		app[i]->SetStopTime (Seconds (SIMTIME));*/
	BulkSendHelper source1 ("ns3::TcpSocketFactory", InetSocketAddress (csmaInterfacesSend.GetAddress (1), sinkPort));
	source1.SetAttribute ("MaxBytes", UintegerValue (10000));
	ApplicationContainer sourceApps1 = source1.Install (csmaNodesSend.Get (1));
	Ptr<BulkSendApplication> app1 = DynamicCast<BulkSendApplication> (sourceApps1.Get(0));
	Ptr<Socket> sock = app1->GetSocket();	
	//sock->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
	sourceApps1.Start (Seconds (0.0));
	sourceApps1.Stop (Seconds (SIMTIME));

	BulkSendHelper source2 ("ns3::TcpSocketFactory", InetSocketAddress (csmaInterfacesSend.GetAddress (2), sinkPort));
	source2.SetAttribute ("MaxBytes", UintegerValue (10000));
	ApplicationContainer sourceApps2 = source2.Install (csmaNodesSend.Get (2));
	sourceApps2.Start (Seconds (0.0));
	sourceApps2.Stop (Seconds (SIMTIME));

	BulkSendHelper source3 ("ns3::TcpSocketFactory", InetSocketAddress (csmaInterfacesSend.GetAddress (3), sinkPort));
	source3.SetAttribute ("MaxBytes", UintegerValue (10000));
	ApplicationContainer sourceApps3 = source3.Install (csmaNodesSend.Get (3));
	sourceApps3.Start (Seconds (0.0));
	sourceApps3.Stop (Seconds (SIMTIME));

	//NS_LOG_UNCOND("pwong " << csmaInterfacesSend.GetAddress (1));

	//csmaDevicesReceive.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

	Simulator::Stop (Seconds (SIMTIME));
	Simulator::Run ();
	Simulator::Destroy ();

	// finding the goodput and printing it to a file
	/*std::ofstream outfile;
  	outfile.open ("goodput.dat");
	for(i = 0; i < NUM; i++)
	{
		sink[i] = DynamicCast<PacketSink> (sinkApps[i].Get (0));
		outfile << "Total Bytes Received " << i + 1 << ": " << sink[i]->GetTotalRx () << std::endl;
		outfile << "Time running " << i + 1 << ": " << app[i]->getTotalTime () << std::endl;
		outfile << "Goodput " << i + 1 << ": " << sink[i]->GetTotalRx () / app[i]->getTotalTime () << " B/s" << std::endl;
	}
	outfile.close();*/

	return 0;
}

