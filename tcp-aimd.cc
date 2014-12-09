#define NS_LOG_APPEND_CONTEXT \
  if (m_node) { std::clog << Simulator::Now ().GetSeconds () << " [node " << m_node->GetId () << "] "; }

#include "tcp-aimd.h"
#include "ns3/log.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/node.h"

NS_LOG_COMPONENT_DEFINE ("TcpAIMD");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TcpAIMD);

TypeId
TcpAIMD::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpAIMD")
    .SetParent<TcpSocketBase> ()
    .AddConstructor<TcpAIMD> ()
    .AddAttribute ("ReTxThreshold", "Threshold for fast retransmit",
                    UintegerValue (3),
                    MakeUintegerAccessor (&TcpAIMD::m_retxThresh),
                    MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("Alpha", "Additive increase rate",
                    UintegerValue (3),
                    MakeUintegerAccessor (&TcpAIMD::m_alpha),
                    MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("Beta", "Multiplicative decrease rate",
                    UintegerValue (3),
                    MakeUintegerAccessor (&TcpAIMD::m_beta),
                    MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("CongestionWindow",
                     "The TCP connection's congestion window",
                     MakeTraceSourceAccessor (&TcpAIMD::m_cWnd))
  ;
  return tid;
}

TcpAIMD::TcpAIMD (void) : m_retxThresh (3), m_alpha (1), m_beta (2)
{
  NS_LOG_FUNCTION (this);
}

TcpAIMD::TcpAIMD (const TcpAIMD& sock)
  : TcpSocketBase (sock),
    m_cWnd (sock.m_cWnd),
	m_ssThresh (sock.m_ssThresh),
    m_initialCWnd (sock.m_initialCWnd),
    m_retxThresh (sock.m_retxThresh),
 	m_alpha (1),
	m_beta (2)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the copy constructor");
}

TcpAIMD::~TcpAIMD (void)
{
}

/* We initialize m_cWnd from this function, after attributes initialized */
int
TcpAIMD::Listen (void)
{
  NS_LOG_FUNCTION (this);
  InitializeCwnd ();
  return TcpSocketBase::Listen ();
}

/* We initialize m_cWnd from this function, after attributes initialized */
int
TcpAIMD::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  InitializeCwnd ();
  return TcpSocketBase::Connect (address);
}

/* Limit the size of in-flight data by cwnd and receiver's rxwin */
uint32_t
TcpAIMD::Window (void)
{
  NS_LOG_FUNCTION (this);
  return std::min (m_rWnd.Get (), m_cWnd.Get ());
}

Ptr<TcpSocketBase>
TcpAIMD::Fork (void)
{
  return CopyObject<TcpAIMD> (this);
}

/* New ACK (up to seqnum seq) received. Increase cwnd and call TcpSocketBase::NewAck() */
void
TcpAIMD::NewAck (const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION (this << seq);
  NS_LOG_LOGIC ("TcpAIMD receieved ACK for seq " << seq <<
                " cwnd " << m_cWnd);

  m_cWnd += m_alpha;
  NS_LOG_INFO ("Updated to cwnd " << m_cWnd);

  TcpSocketBase::NewAck (seq);
}

void
TcpAIMD::DupAck (const TcpHeader& t, uint32_t count)
{
  NS_LOG_FUNCTION (this << "t " << count);
  m_cWnd /= m_beta;
  NS_LOG_INFO ("Triple dupack. Reset cwnd to " << m_cWnd);
  DoRetransmit ();
}

// Retransmit timeout
void TcpAIMD::Retransmit (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());

  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // If all data are received (non-closing socket and nothing to send), just return
  if (m_state <= ESTABLISHED && m_txBuffer.HeadSequence () >= m_highTxMark) return;

  // According to RFC2581 sec.3.1, upon RTO, ssthresh is set to half of flight
  // size and cwnd is set to 1*MSS, then the lost packet is retransmitted and
  // TCP back to slow start
  m_cWnd /= m_beta;
  m_nextTxSequence = m_txBuffer.HeadSequence (); // Restart from highest Ack
  NS_LOG_INFO ("RTO. Reset cwnd to " << m_cWnd <<
               ", restart from seqnum " << m_nextTxSequence);
  m_rtt->IncreaseMultiplier ();             // Double the next RTO
  DoRetransmit ();                          // Retransmit the packet
}

void
TcpAIMD::SetSegSize (uint32_t size)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpAIMD::SetSegSize() cannot change segment size after connection started.");
  m_segmentSize = size;
}

void TcpAIMD::SetAlpha (uint32_t alpha)
{
  m_alpha = alpha;
}

void TcpAIMD::SetBeta (uint32_t beta)
{
  m_beta = beta;
}

void
TcpAIMD::SetInitialCwnd (uint32_t cwnd)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpAIMD::SetInitialCwnd() cannot change initial cwnd after connection started.");
  m_initialCWnd = cwnd;
}

void
TcpAIMD::SetSSThresh (uint32_t threshold)
{
  m_ssThresh = threshold;
}

uint32_t
TcpAIMD::GetSSThresh (void) const
{
  return m_ssThresh;
}

uint32_t
TcpAIMD::GetInitialCwnd (void) const
{
  return m_initialCWnd;
}

uint32_t TcpAIMD::GetAlpha ()
{
  return m_alpha;
}

uint32_t TcpAIMD::GetBeta ()
{
  return m_beta;
}

void 
TcpAIMD::InitializeCwnd (void)
{
  m_cWnd = m_initialCWnd * m_segmentSize;
}

} // namespace ns3
