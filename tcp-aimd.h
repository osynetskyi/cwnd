#ifndef TCP_AIMD_H
#define TCP_AIMD_H

#include "tcp-socket-base.h"

namespace ns3 {

class TcpAIMD : public TcpSocketBase
{
public:
  static TypeId GetTypeId (void);

  TcpAIMD (void);

  TcpAIMD (const TcpAIMD& sock);
  virtual ~TcpAIMD (void);

  // From TcpSocketBase
  virtual int Connect (const Address &address);
  virtual int Listen (void);

protected:
  virtual uint32_t Window (void); // Return the max possible number of unacked bytes
  virtual Ptr<TcpSocketBase> Fork (void); // Call CopyObject<TcpAIMD> to clone me
  virtual void NewAck (const SequenceNumber32& seq); // Inc cwnd and call NewAck() of parent
  virtual void DupAck (const TcpHeader& t, uint32_t count);  // Fast retransmit
  virtual void Retransmit (void); // Retransmit timeout

  // Implementing ns3::TcpSocket -- Attribute get/set
  virtual void     SetSegSize (uint32_t size);
  virtual void     SetSSThresh (uint32_t threshold);
  virtual uint32_t GetSSThresh (void) const;
  virtual void     SetInitialCwnd (uint32_t cwnd);
  virtual uint32_t GetInitialCwnd (void) const;

  void SetAlpha (uint32_t alpha);
  void SetBeta (uint32_t beta);
  uint32_t GetAlpha();
  uint32_t GetBeta();

private:
  /**
   * \brief Set the congestion window when connection starts
   */
  void InitializeCwnd (void);

protected:
  TracedValue<uint32_t>  m_cWnd;         //!< Congestion window
  uint32_t               m_ssThresh;     //!< Slow Start Threshold
  uint32_t               m_initialCWnd;  //!< Initial cWnd value
  uint32_t               m_retxThresh;   //!< Fast Retransmit threshold
  uint32_t               m_alpha;        //!< AIMD linear growth rate
  uint32_t               m_beta;         //!< AIMD multiplicative decrease rate
};

} // namespace ns3

#endif /* TCP_AIMD_H */
