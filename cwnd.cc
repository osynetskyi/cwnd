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
//#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
//#include "ns3/random-variable-stream.h"
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
#define SIMTIME 170

// number of different strategies
#define NUM 3

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CWnd");

class MyApp : public Application 
{
	public:
		MyApp ();
		virtual ~MyApp();
		
		void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
		double getTotalTime();

	private:
		virtual void StartApplication (void);
		virtual void StopApplication (void);

		void ScheduleTx (void);
		void SendPacket (void);

	Ptr<Socket>     m_socket;
	Address         m_peer;
	uint32_t        m_packetSize;
	uint32_t        m_nPackets;
	DataRate        m_dataRate;
	EventId         m_sendEvent;
	bool            m_running;
	uint32_t        m_packetsSent;
	double m_startTime;
	double m_stopTime;
};

MyApp::MyApp ()
	: m_socket (0), 
	m_peer (), 
	m_packetSize (0), 
	m_nPackets (0), 
	m_dataRate (0), 
	m_sendEvent (), 
	m_running (false), 
	m_packetsSent (0),
	m_startTime (0),
	m_stopTime (0)
{
}

MyApp::~MyApp()
{
	m_socket = 0;
}

double MyApp::getTotalTime() 
{
	return m_stopTime - m_startTime;
}

void MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, 
					uint32_t nPackets, DataRate dataRate)
{
	m_socket = socket;
	m_peer = address;
	m_packetSize = packetSize;
	m_nPackets = nPackets;
	m_dataRate = dataRate;
	m_startTime = 0;
	m_stopTime = 0;
}

void MyApp::StartApplication (void)
{
	m_running = true;
	m_packetsSent = 0;
	m_startTime = Simulator::Now ().GetSeconds ();
	m_socket->Bind ();
	m_socket->Connect (m_peer);
	SendPacket ();
}

void MyApp::StopApplication (void)
{
	m_running = false;
	if (m_stopTime == 0) 
	{
		m_stopTime = Simulator::Now ().GetSeconds ();
	}

	if (m_sendEvent.IsRunning ())
	{
		Simulator::Cancel (m_sendEvent);
	}

	if (m_socket)
	{
		m_socket->Close ();
	}
}

void MyApp::SendPacket (void)
{
	Ptr<Packet> packet = Create<Packet> (m_packetSize);
	m_socket->Send (packet);

	if (++m_packetsSent < m_nPackets)
	{
		ScheduleTx ();
	} else {
		m_stopTime = Simulator::Now ().GetSeconds ();
	}
}

void MyApp::ScheduleTx (void)
{
	if (m_running)
	{
		Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
		m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
	}
}

static void CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
	// GetContext is used to find a NodeId of a node that triggered the event
	NS_LOG_UNCOND ("Sender " << Simulator::GetContext () - 1 << " " << Simulator::Now ().GetSeconds () << "\t" << newCwnd);
}

static void RxDrop (Ptr<const Packet> p)
{
	NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}

int 
main (int argc, char *argv[])
{
	uint32_t alpha[NUM], beta[NUM];
	int i = 0;
	CommandLine cmd;

	// TODO: add cmd args in a cycle
	cmd.AddValue("alpha_1", "alpha 1", alpha[0]);
	cmd.AddValue("alpha_2", "alpha 2", alpha[1]);
	cmd.AddValue("beta_1", "beta 1", beta[0]);
	cmd.AddValue("beta_2", "beta 2", beta[1]);
	cmd.AddValue("alpha_3", "alpha 3", alpha[2]);
	cmd.AddValue("beta_3", "beta 3", beta[2]);
	cmd.Parse(argc, argv);

	NodeContainer p2pNodes, csmaNodesSend, csmaNodesReceive;
	p2pNodes.Create (2);
	csmaNodesSend.Add(p2pNodes.Get (0));
	csmaNodesSend.Create (NUM);
	csmaNodesReceive.Add(p2pNodes.Get (1));
	csmaNodesReceive.Create (NUM);

	// bottleneck link
	PointToPointHelper pointToPoint;
	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("7Mbps"));
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
	stack.Install (csmaNodesSend);
	stack.Install (csmaNodesReceive);

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

	Ptr<MyApp> app[NUM];
	Ptr<PacketSink> sink[NUM];

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
		ns3TcpSocket[i]->SetAttribute("SegmentSize", UintegerValue(alpha[i]));
		ns3TcpSocket[i]->SetAttribute("SlowStartThreshold", UintegerValue(beta[i]));
	
		/// setup the data-sending apps on sender nodes
		app[i] = CreateObject<MyApp> ();
		app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 100000, DataRate ("5Mbps"));
		csmaNodesSend.Get (i + 1)->AddApplication (app[i]);
		app[i]->SetStartTime (Seconds (0.));
		app[i]->SetStopTime (Seconds (SIMTIME));
	}

	//csmaDevicesReceive.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

	Simulator::Stop (Seconds (SIMTIME));
	Simulator::Run ();
	Simulator::Destroy ();

	// finding the goodput and printing it to a file
	std::ofstream outfile;
  	outfile.open ("goodput.dat");
	for(i = 0; i < NUM; i++)
	{
		sink[i] = DynamicCast<PacketSink> (sinkApps[i].Get (0));
		//std::cout << "Total Bytes Received " << i + 1 << ": " << sink[i]->GetTotalRx () << std::endl;
		//std::cout << "Time running " << i + 1 << ": " << app[i]->getTotalTime () << std::endl;
		outfile << "Goodput " << i + 1 << ": " << sink[i]->GetTotalRx () / app[i]->getTotalTime () << " B/s" << std::endl;
	}
	outfile.close();

	return 0;
}

