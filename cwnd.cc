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
#define SIMTIME 100

// number of different strategies
#define NUM 3

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

/*void MyApp::StartApplication (void) 
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
                          "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind6 ();
        }
      else if (InetSocketAddress::IsMatchingType (m_peer))
       {
          m_socket->Bind ();
        }

      m_socket->Connect (m_peer);
      m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (
        MakeCallback (&BulkSendApplication::ConnectionSucceeded, this),
        MakeCallback (&BulkSendApplication::ConnectionFailed, this));
      m_socket->SetSendCallback (
        MakeCallback (&BulkSendApplication::DataSend, this));
    }
  if (m_connected)
    {
      SendData ();
    }
}*/

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

void MyApp::StopApplication (void) // Called at time specified by Stop
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
    { // Time to send more
      uint32_t toSend = m_packetSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (m_packetSize, m_maxBytes - m_totBytes);
        }
      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      //m_txTrace (packet);
      int actual = m_socket->Send (packet);
      if (actual > 0)
        {
          m_totBytes += actual;
        }
      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed ip.
      if ((unsigned)actual != toSend)
        {
          break;
        }
    }
  // Check if time to close (all sent)
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



/*void MyApp::StartApplication (void)
{
	m_running = true;
	m_totBytes = 0;
	m_startTime = Simulator::Now ().GetSeconds ();
	m_socket->Bind ();
	m_socket->Connect (m_peer);
	SendData ();
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
}*/

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
	NS_LOG_UNCOND ("Rtt " << num << " " << newRtt.GetSeconds());
	NS_LOG_UNCOND ("Approx. throughput " << num << " " << Simulator::Now ().GetSeconds () << "\t" 
					<< cwnds[num-1] / newRtt.GetSeconds() * 8 / 1024 / 1024);
}

/*static void RxDrop (Ptr<const Packet> p)
{
	NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}*/

int 
main (int argc, char *argv[])
{
	uint32_t alpha[NUM], beta[NUM];
	int i = 0;
	char buf_a[7];
	char buf_b[6];
	CommandLine cmd;

	for(i = 0; i < NUM; i++)
	{
		sprintf(buf_a, "alpha_%d", i + 1);
		cmd.AddValue(buf_a, buf_a, alpha[i]);		
		sprintf(buf_b, "beta_%d", i + 1);		
		cmd.AddValue(buf_b, buf_b, beta[i]);
		cwnds[i] = 0;
	}
	cmd.Parse(argc, argv);

	/*alpha[0] = 1000;
	alpha[1] = 2000;
	alpha[2] = 3000;
	beta[0] = 0;
	beta[1] = 0;
	beta[2] = 0;*/

	NodeContainer p2pNodes, csmaNodesSend, csmaNodesReceive;
	p2pNodes.Create (2);
	csmaNodesSend.Add(p2pNodes.Get (0));
	csmaNodesSend.Create (NUM);
	csmaNodesReceive.Add(p2pNodes.Get (1));
	csmaNodesReceive.Create (NUM);

	// bottleneck link
	PointToPointHelper pointToPoint;
	//pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("7Mbps"));
	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));
	//pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
	CsmaHelper csmaHelperSend, csmaHelperReceive;
	//csmaHelper.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
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
		ns3TcpSocket[i]->SetAttribute("SlowStartThreshold", UintegerValue(beta[i]));
	
		/// setup the data-sending apps on sender nodes
		app[i] = CreateObject<MyApp> ();
		//app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 100000, DataRate ("5Mbps"));
		//app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 10000, DataRate ("4Mbps"));
		app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 1000000);
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
		outfile << "Total Bytes Received " << i + 1 << ": " << sink[i]->GetTotalRx () << std::endl;
		outfile << "Time running " << i + 1 << ": " << app[i]->getTotalTime () << std::endl;
		outfile << "Goodput " << i + 1 << ": " << sink[i]->GetTotalRx () / app[i]->getTotalTime () << " B/s" << std::endl;
	}
	outfile.close();

	return 0;
}

