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
#include "ns3/flow-monitor-module.h"

// simulation time
#define SIMTIME 140

// number of different strategies
#define NUM 2

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CWnd");

// global variable for CWnds
uint32_t cwnds[NUM];

class MyApp : public Application 
{
	public:
		MyApp ();
		virtual ~MyApp();
		
		void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t maxBytes);
		double getTotalTime();
		void SendData ();
		void DataSend (Ptr<Socket>, uint32_t);
		void ConnectionSucceeded(Ptr<Socket>);
		void ConnectionFailed(Ptr<Socket>);

	private:
		virtual void StartApplication (void);
		virtual void StopApplication (void);

	Ptr<Socket>     m_socket;
	Address         m_peer;
	uint32_t        m_packetSize;
	uint32_t		m_maxBytes;
	uint32_t		m_totBytes;
	EventId         m_sendEvent;
	bool            m_running;
	double 			m_startTime;
	double 			m_stopTime;
};

MyApp::MyApp ()
	: m_socket (0), 
	m_peer (), 
	m_totBytes (0), 
	m_sendEvent (), 
	m_running (false), 
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

void MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t maxBytes)
{
	m_socket = socket;
	m_peer = address;
	m_packetSize = packetSize;
	m_maxBytes = maxBytes;
	m_startTime = 0;
	m_stopTime = 0;
}

void MyApp::StartApplication (void)
{
	m_running = true;
	m_totBytes = 0;
	m_startTime = Simulator::Now ().GetSeconds ();
	m_socket->Connect (m_peer);
    m_socket->ShutdownRecv ();
    m_socket->SetConnectCallback (
    	MakeCallback (&MyApp::ConnectionSucceeded, this),
    	MakeCallback (&MyApp::ConnectionFailed, this));
    m_socket->SetSendCallback (
        MakeCallback (&MyApp::DataSend, this));
  if (m_running)
    {
      SendData ();
    }
}

void MyApp::StopApplication (void) 
{
  	m_running = false;
	if (m_stopTime == 0) 
	{
		m_stopTime = Simulator::Now ().GetSeconds ();
	}

	if (m_socket)
	{
		m_socket->Close ();
	}
}

void MyApp::SendData (void)
{
  NS_LOG_FUNCTION (this);

  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
      uint32_t toSend = m_packetSize;
      if (m_maxBytes > 0)
        {
          toSend = std::min (m_packetSize, m_maxBytes - m_totBytes);
        }
      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      int actual = m_socket->Send (packet);
      if (actual > 0)
        {
          m_totBytes += actual;
        }

      if ((unsigned)actual != toSend)
        {
          break;
        }
    }
  if (m_totBytes == m_maxBytes && m_running)
    {
      StopApplication();
    }
}

void MyApp::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("Connection succeeded");
  m_running = true;
  SendData ();
}

void MyApp::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("Connection Failed");
}

void MyApp::DataSend (Ptr<Socket>, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_running)
    { // Only send new data if the connection has completed
      Simulator::ScheduleNow (&MyApp::SendData, this);
    }
}

static void CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
	// GetContext is used to find a NodeId of a node that triggered the event
	int num = Simulator::GetContext () - 1;
	NS_LOG_UNCOND ("Sender " << num << " " << Simulator::Now ().GetSeconds () << "\t" << newCwnd);
	cwnds[num - 1] = newCwnd;
	/*TypeId tid = TypeId::LookupByName ("ns3::TcpReno");
	std::stringstream nodeId;
	nodeId << Simulator::GetContext ();
	std::string specificNode = "/NodeList/" + nodeId.str () + "/$ns3::TcpL4Protocol/SocketType";
	if(Simulator::GetContext () == 2) {
		Config::Set (specificNode, TypeIdValue (tid));
		NS_LOG_UNCOND ("SET!");
	}*/
}

/*static void RwndChange (uint32_t oldRwnd, uint32_t newRwnd)
{
	NS_LOG_UNCOND ("RWND " << Simulator::GetContext () - 1 << " " << oldRwnd << " " << newRwnd);
}*/

static void RttChange (Time oldRtt, Time newRtt)
{
	int num = Simulator::GetContext () - 1;
	NS_LOG_UNCOND ("Rtt " << num << " " << newRtt.GetSeconds() << " relevant cwnd: " << cwnds[num-1]);
	NS_LOG_UNCOND ("Approx. throughput " << num << " " << Simulator::Now ().GetSeconds () << "\t" 
					<< cwnds[num-1] / newRtt.GetSeconds() * 8 / 1000/* / 1024*/);
}

static void Drop (Ptr<const Packet> p)
{
	NS_LOG_UNCOND ("Drop at " << Simulator::Now ().GetSeconds ());
}

int 
main (int argc, char *argv[])
{
	LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
	LogComponentEnable ("TcpTahoe", LOG_LEVEL_LOGIC);
	//LogComponentEnable ("CWnd", LOG_LEVEL_INFO);

	uint32_t alpha[NUM];//, beta[NUM];
	uint32_t bufsize;
	int i = 0;
	char buf_a[7];
	//char buf_b[6];
	CommandLine cmd;

	for(i = 0; i < NUM; i++)
	{
		sprintf(buf_a, "alpha_%d", i + 1);
		cmd.AddValue(buf_a, buf_a, alpha[i]);		
		/*sprintf(buf_b, "beta_%d", i + 1);		
		cmd.AddValue(buf_b, buf_b, beta[i]);*/
		cwnds[i] = 0;
	}
	cmd.AddValue("bufsize", "bufsize", bufsize);
	cmd.Parse(argc, argv);

	/*alpha[0] = 1000;
	alpha[1] = 2000;
	//alpha[2] = 3000;
	beta[0] = 0;
	beta[1] = 0;
	//beta[2] = 0;*/

	NodeContainer p2pNodes, csmaNodesSend, csmaNodesReceive;
	p2pNodes.Create (2);
	csmaNodesSend.Add(p2pNodes.Get (0));
	csmaNodesSend.Create (NUM);
	csmaNodesReceive.Add(p2pNodes.Get (1));
	csmaNodesReceive.Create (NUM);

	// bottleneck link
	PointToPointHelper pointToPoint;
	//pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("7Mbps"));
	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
	//pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (100)));
	//pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
	CsmaHelper csmaHelperSend, csmaHelperReceive;

	/*CsmaHelper::SetQueue("ns3::RedQueue", 
                     "Mode", StringValue("QUEUE_MODE_BYTES"), 
                     "QueueLimit", StringValue("10MB"));*/
	//csmaHelperSend.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
	//csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
	//csmaHelper.SetDeviceAttribute ("Mtu", UintegerValue (9000));

	NetDeviceContainer p2pDevices, csmaDevicesSend, csmaDevicesReceive;
	p2pDevices = pointToPoint.Install (p2pNodes);
	csmaDevicesSend = csmaHelperSend.Install (csmaNodesSend);
	csmaDevicesReceive = csmaHelperReceive.Install (csmaNodesReceive);

	//Config::Set("/NodeList/0/DeviceList/*/TxQueue/MaxBytes", UintegerValue(6553500));
	//Config::Set("/NodeList/0/DeviceList/*/TxQueue/MaxPackets", UintegerValue(25));
	Config::Set("/NodeList/0/DeviceList/*/TxQueue/MaxPackets", UintegerValue(bufsize));
	//Config::Set("/NodeList/1/DeviceList/*/TxQueue/MaxPackets", UintegerValue(1));

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

	TypeId tid = TypeId::LookupByName ("ns3::TcpTahoe");
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
		//ns3TcpSocket[i]->TraceConnectWithoutContext ("RWND", MakeCallback (&RwndChange));
		ns3TcpSocket[i]->TraceConnectWithoutContext ("RTT", MakeCallback (&RttChange));
		ns3TcpSocket[i]->SetAttribute("SegmentSize", UintegerValue(alpha[i]));
		//ns3TcpSocket[i]->SetAttribute("SlowStartThreshold", UintegerValue(beta[i]));
	
		/// setup the data-sending apps on sender nodes
		app[i] = CreateObject<MyApp> ();
		//app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 100000, DataRate ("5Mbps"));
		//app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 10000, DataRate ("4Mbps"));
		app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 8000000);
		csmaNodesSend.Get (i + 1)->AddApplication (app[i]);
		app[i]->SetStartTime (Seconds (0.));
		app[i]->SetStopTime (Seconds (SIMTIME));
	}
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll();

	/*app[1]->SetStartTime (Seconds (40.));
	app[2]->SetStartTime (Seconds (80.));*/

	p2pDevices.Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&Drop));
	Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Mac/MacTxDrop", MakeCallback(&Drop));
	Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Phy/PhyRxDrop", MakeCallback(&Drop));
	Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Phy/PhyTxDrop", MakeCallback(&Drop));

	Simulator::Stop (Seconds (SIMTIME));
	Simulator::Run ();

	monitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);

      if ((t.sourceAddress == Ipv4Address("1.1.1.2") && t.destinationAddress == Ipv4Address("3.3.3.2"))
    	|| (t.sourceAddress == Ipv4Address("1.1.1.3") && t.destinationAddress == Ipv4Address("3.3.3.3"))
    	|| (t.sourceAddress == Ipv4Address("1.1.1.4") && t.destinationAddress == Ipv4Address("3.3.3.4")))
        {
    	  NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
    	  NS_LOG_UNCOND("Tx Packets = " << iter->second.txPackets);
    	  NS_LOG_UNCOND("Rx Packets = " << iter->second.rxPackets);
    	  NS_LOG_UNCOND("Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1000  << " Kbps");
        }
    }
	monitor->SerializeToXmlFile("cwnd.flowmon", true, true);


	Simulator::Destroy ();

	// finding the goodput and printing it to a file
	std::ofstream outfile;
  	outfile.open ("goodput.dat");
	for(i = 0; i < NUM; i++)
	{
		sink[i] = DynamicCast<PacketSink> (sinkApps[i].Get (0));
		outfile << "Total Bytes Received " << i + 1 << ": " << sink[i]->GetTotalRx () << std::endl;
		outfile << "Time running " << i + 1 << ": " << app[i]->getTotalTime () << std::endl;
		outfile << "Goodput " << i + 1 << ": " << sink[i]->GetTotalRx () / app[i]->getTotalTime () << " B/s" << std::endl;
	}
	outfile.close();

	return 0;
}

