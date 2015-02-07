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

#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
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

#define TOTALTIME 4500

// number of different strategies
#define NUM 2

#define NUMPROTO 4

using namespace ns3;

NodeContainer p2pNodes, csmaNodesSend, csmaNodesReceive;
PointToPointHelper pointToPoint;
CsmaHelper csmaHelperSend, csmaHelperReceive;
NetDeviceContainer p2pDevices, csmaDevicesSend, csmaDevicesReceive;
InternetStackHelper stack;
Ipv4AddressHelper address;
Ipv4InterfaceContainer p2pInterfaces, csmaInterfacesSend, csmaInterfacesReceive;
FlowMonitorHelper flowmon;
Ptr<FlowMonitor> monitor;
const char *algs[4] = {"ns3::TcpWestwood", "ns3::TcpReno", "ns3::TcpTahoe", "ns3::TcpNewReno"};

NS_LOG_COMPONENT_DEFINE ("Arbiter");


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
	//NS_LOG_UNCOND ("Sender " << num << " " << Simulator::Now ().GetSeconds () << "\t" << newCwnd);
	cwnds[num - 1] = newCwnd;
}

static void RttChange (Time oldRtt, Time newRtt)
{
	//int num = Simulator::GetContext () - 1;
	//NS_LOG_UNCOND ("Rtt " << num << " " << newRtt.GetSeconds() << " relevant cwnd: " << cwnds[num-1]);
	//NS_LOG_UNCOND ("Approx. throughput " << num << " " << Simulator::Now ().GetSeconds () << "\t" 
					//<< cwnds[num-1] / newRtt.GetSeconds() * 8 / 1000/* / 1024*/);
}

/*static void Drop (Ptr<const Packet> p)
{
	NS_LOG_UNCOND ("Drop at " << Simulator::Now ().GetSeconds ());
}*/





class Arbiter : public Application
{
public:
  Arbiter ()
  {
     m_node = NULL;
     m_startTime = Seconds(0.0);
	 iteration = 0;
     m_stopTime = Seconds(TOTALTIME);
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
	double thp[4];

  	void StartApplication (void) {
  
	p2pNodes.Create (2);
	csmaNodesSend.Add(p2pNodes.Get (0));
	csmaNodesSend.Create (NUM);
	csmaNodesReceive.Add(p2pNodes.Get (1));
	csmaNodesReceive.Create (NUM);

	// bottleneck link
	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));

	p2pDevices = pointToPoint.Install (p2pNodes);
	csmaDevicesSend = csmaHelperSend.Install (csmaNodesSend);
	csmaDevicesReceive = csmaHelperReceive.Install (csmaNodesReceive);

	//Config::Set("/NodeList/0/DeviceList/*/TxQueue/MaxPackets", UintegerValue(bufsize));

	stack.Install (csmaNodesSend);
	stack.Install (csmaNodesReceive);

	address.SetBase ("2.2.2.0", "255.255.255.0");
	p2pInterfaces = address.Assign (p2pDevices);
	address.SetBase ("1.1.1.0", "255.255.255.0");
	csmaInterfacesSend = address.Assign (csmaDevicesSend);
	address.SetBase ("3.3.3.0", "255.255.255.0");
	csmaInterfacesReceive = address.Assign (csmaDevicesReceive);

	// enable routing
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	//Setup("ns3::TcpTahoe");
	//Setup(algs[iteration]);
	Simulator::Schedule (Seconds(0.0), &Arbiter::Setup, this, algs[iteration]);
  }

void Setup(const char *name) {

	//std::cout << Simulator::Now().GetSeconds() << std::endl;

	uint16_t sinkPort = 8080;
	Address sinkAddress[NUM];
	ApplicationContainer sinkApps[NUM];
	PacketSinkHelper *packetSinkHelper[NUM];

	//TypeId tid = TypeId::LookupByName ("ns3::TcpTahoe");
	TypeId tid = TypeId::LookupByName (name);
	std::stringstream nodeId[NUM];
	std::string specificNode[NUM];
	Ptr<Socket> ns3TcpSocket[NUM];
	//Ptr<Socket> ns3TcpSocket2[NUM];

	Ptr<MyApp> app[NUM];
	//Ptr<MyApp> app2[NUM];
	//Ptr<PacketSink> sink[NUM];

  //std::cout << "Setup started at: " << Simulator::Now().GetSeconds() << std::endl;
  std::cout << "Using " << name << std::endl;


	for(int i = 0; i < NUM; i++)
	{
		// create packet sinks on receiving nodes
		sinkAddress[i] = InetSocketAddress (csmaInterfacesReceive.GetAddress (i + 1), sinkPort);
		packetSinkHelper[i] = new PacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
		sinkApps[i] = packetSinkHelper[i]->Install (csmaNodesReceive.Get (i + 1));
		sinkApps[i].Start (Seconds (1.));
		sinkApps[i].Stop (Seconds (SIMTIME + 1.));
	
		// open sockets on senders and set their attributes
		nodeId[i] << csmaNodesSend.Get (i + 1)->GetId ();
		specificNode[i] = "/NodeList/" + nodeId[i].str () + "/$ns3::TcpL4Protocol/SocketType";
		
		// specific algorithm for first user
		if(i == 0) {
			Config::Set (specificNode[i], TypeIdValue (tid));
		} else {
			Config::Set (specificNode[i], TypeIdValue (TypeId::LookupByName ("ns3::TcpWestwood")));
		}

		ns3TcpSocket[i] = Socket::CreateSocket (csmaNodesSend.Get (i + 1), TcpSocketFactory::GetTypeId ());
		ns3TcpSocket[i]->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
		ns3TcpSocket[i]->TraceConnectWithoutContext ("RTT", MakeCallback (&RttChange));
		//ns3TcpSocket[i]->SetAttribute("SegmentSize", UintegerValue(alpha[i]));
		ns3TcpSocket[i]->SetAttribute("SlowStartThreshold", UintegerValue(0));
	
		// setup the data-sending apps on sender nodes
		app[i] = CreateObject<MyApp> ();
		app[i]->Setup (ns3TcpSocket[i], sinkAddress[i], 1040, 8000000);
		csmaNodesSend.Get (i + 1)->AddApplication (app[i]);
		app[i]->SetStartTime (Seconds (1.));
		app[i]->SetStopTime (Seconds (SIMTIME + 1.));
	}

	monitor = flowmon.InstallAll();
	Simulator::Schedule (Seconds(SIMTIME + 1), &Arbiter::getStats, this, name);
	//std::cout << "Setup exit at: " << Simulator::Now().GetSeconds() << std::endl;
	//Simulator::Schedule (Seconds(12.0), &Arbiter::Setup, this, "ns3::TcpReno");
}

/*void loop(std::map<FlowId, FlowMonitor::FlowStats> stats)
{
	if (stats[2*iteration -1].rxBytes >= 1200) {
		printf("lol!\n");
		//Setup(2000);
	} else {
		printf("pwn!\n");
		//Setup(1000);
	}
}*/

void getStats(const char *name)
{
	//std::cout << "getStats started at: " << Simulator::Now().GetSeconds() << std::endl;
    monitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
	printf("Rx1: %lld\n", stats[4*iteration + 1].rxBytes);
	printf("Rx2: %lld\n", stats[4*iteration + 2].rxBytes);
	printf("Total time sending1: %f\n", stats[4*iteration + 1].timeLastRxPacket.GetSeconds() - stats[4*iteration + 1].timeFirstTxPacket.GetSeconds());
	printf("Total time sending2: %f\n", stats[4*iteration + 2].timeLastRxPacket.GetSeconds() - stats[4*iteration + 2].timeFirstTxPacket.GetSeconds());
	printf("Thp1: %f\n", stats[4*iteration + 1].rxBytes / (stats[4*iteration + 1].timeLastRxPacket.GetSeconds() - stats[4*iteration + 1].timeFirstTxPacket.GetSeconds()));
	printf("Thp2: %f\n", stats[4*iteration + 2].rxBytes / (stats[4*iteration + 2].timeLastRxPacket.GetSeconds() - stats[4*iteration + 2].timeFirstTxPacket.GetSeconds()));
	monitor->SerializeToXmlFile(name, true, true);
	//printf("Bytes: %lld\n", stats[1].rxBytes);
	//loop(stats);
	//Simulator::Schedule (Seconds(1.0), &Arbiter::Setup, this, "ns3::TcpReno");
	thp[iteration] = stats[4*iteration + 1].rxBytes / (stats[4*iteration + 1].timeLastRxPacket.GetSeconds() - stats[4*iteration + 1].timeFirstTxPacket.GetSeconds());
	iteration++;
	if(iteration < 4) {
		Simulator::Schedule (Seconds(100.0), &Arbiter::Setup, this, algs[iteration]);
	}
	//std::cout << "getStats exit at: " << Simulator::Now().GetSeconds() << std::endl;
}

	void StopApplication (void) {
		for(int i = 0; i < 4; i++) {
			printf("Thp[%d] = %f\n", i, thp[i]);
		}
	}

};






int
main (int argc, char *argv[])
{
    //init();
    
	Arbiter arb1;
	arb1.Initialize();

    //apps();

  Simulator::Stop(Seconds(TOTALTIME));
  Simulator::Run ();
  Simulator::Destroy ();
	//printf("res %d\n", nodes.GetN());
  return 0;
}
