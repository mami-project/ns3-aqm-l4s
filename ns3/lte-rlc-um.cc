/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Manuel Requena <manuel.requena@cttc.es>
 */

#include "ns3/simulator.h"
#include "ns3/log.h"

#include "ns3/lte-rlc-header.h"
#include "ns3/lte-rlc-um.h"
#include "ns3/lte-rlc-sdu-status-tag.h"
#include "ns3/lte-rlc-tag.h"

#include "ns3/ipv4-header.h"
#include "ns3/lte-pdcp-header.h"
#include "ns3/tcp-header.h"

#include "ns3/packet.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LteRlcUm");

NS_OBJECT_ENSURE_REGISTERED (LteRlcUm);

LteRlcUm::LteRlcUm ()
  : m_maxTxBufferSize (10000 * 1502),
    m_txBufferSize (0),
    m_sequenceNumber (0),
    m_vrUr (0),
    m_vrUx (0),
    m_vrUh (0),
    m_windowSize (512),
    m_expectedSeqNumber (0)
{
  NS_LOG_FUNCTION (this);
  m_reassemblingState = WAITING_S0_FULL;
  
  
  // AQM
  m_uv = CreateObject<UniformRandomVariable> ();
  m_secToken = 0;
  m_dropProb = 0;
  m_qIsInit = false;
  m_fIsInit = false;
  m_queueHandle = this;
  m_qDelayFile = new std::ofstream();
  m_throughputFile = new std::ofstream();
  m_actionsFile = new std::ofstream();
  
  // RED
  m_minThRED = 5;
  m_maxThRED = 15;
  m_maxProbRED = 0.02;
  
  // PIE
  // only use if PIE
  m_aPIE = 0.125;
  m_bPIE = 1.25;
  m_qDelayRefPIE = 0.015;
  m_tUpdatePIE = 0.015;
  m_qDelayOldPIE = 0;
  m_burstAllowancePIE = 0;
  m_maxBurstAllowancePIE = 0.15;
  m_throughputCount = 0;
  
  m_k = 2;
  
  // true for coupled
  // false for uncoupled
  m_l4s = false;
  m_red = false;
  
  m_enqueuedBytes = 0;
}

LteRlcUm::~LteRlcUm ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
LteRlcUm::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LteRlcUm")
    .SetParent<LteRlc> ()
    .SetGroupName("Lte")
    .AddConstructor<LteRlcUm> ()
    .AddAttribute ("MaxTxBufferSize",
                   "Maximum Size of the Transmission Buffer (in Bytes)",
                   UintegerValue (10240 * 1024),
                   MakeUintegerAccessor (&LteRlcUm::m_maxTxBufferSize),
                   MakeUintegerChecker<uint32_t> ())
    ;
  return tid;
}

void
LteRlcUm::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  if ((uint32_t) m_lcid == 5)
    {
	  m_qDelayFile->close ();
	  m_queueHandle->m_qDelayFile->close ();
	  m_actionsFile->close ();
	  m_queueHandle->m_actionsFile->close ();
	  
	  m_throughputFile->close ();
	}
  if ((uint32_t) m_lcid == 4)
    {
	  m_qDelayFile->close ();
	  m_queueHandle->m_qDelayFile->close ();
	  m_actionsFile->close ();
	  m_queueHandle->m_actionsFile->close ();
	  
	  m_queueHandle->m_throughputFile->close ();
	}
  
  m_reorderingTimer.Cancel ();
  m_rbsTimer.Cancel ();

  LteRlc::DoDispose ();
}

/**
 * RLC SAP
 */

double
LteRlcUm::GetQueueDelay (Ptr<LteRlcUm> handle)
{
  if (!handle->m_txBuffer.empty())
	{
	  double now = Simulator::Now ().GetSeconds ();
	  PacketTagIterator it = handle->m_txBuffer.front ()->GetPacketTagIterator ();
	  RlcTag rlc;
	  it.Next ();
	  it.Next ().GetTag (rlc);
	  return now - rlc.GetSenderTimestamp ().GetSeconds ();
	}
  return 0.0;
}

double
LteRlcUm::GetTimeStamp (Ptr<Packet> packet)
{
  PacketTagIterator it = packet->GetPacketTagIterator ();
  RlcTag rlc;
  it.Next ();
  it.Next ().GetTag (rlc);
  return rlc.GetSenderTimestamp ().GetSeconds ();
}

bool
LteRlcUm::DoDropRED (Ptr<Packet> p)
{
  uint32_t qSize = m_txBufferSize;
  if ((uint32_t) m_lcid == 5)
    {
	  if (GetQueueDelay (this) > m_qDelayRefPIE) { return true; } // mark if qDelay of L4S Queue > 20ms
	  qSize = m_queueHandle->m_txBufferSize;
    }
  
  if (qSize >= m_maxThRED) { return true; }
  if (qSize <= m_minThRED) { return false; }
  
  double dropProb = ((double) (qSize - m_minThRED)) / (m_maxThRED - m_minThRED);
  if ((uint32_t) m_lcid == 5) { dropProb = sqrt (dropProb); }
  double u = m_uv->GetValue ();
  
  if (u < dropProb) { return true; }
  return false;
}

void
LteRlcUm::CalculateP ()
{
  double qDelay = GetQueueDelay (this);
  
  double dropProb = m_aPIE * (qDelay - m_qDelayRefPIE) + m_bPIE * (qDelay - m_qDelayOldPIE);
  
  if (m_dropProb < 0.000001) { dropProb /= 2048; }
  else if (m_dropProb < 0.00001) { dropProb /= 512; }
  else if (m_dropProb < 0.0001) { dropProb /= 128; }
  else if (m_dropProb < 0.001) { dropProb /= 32; }
  else if (m_dropProb < 0.01) { dropProb /= 8; }
  else if (m_dropProb < 0.1) { dropProb /= 2; }

  m_dropProb += dropProb;

  if (qDelay == 0 && m_qDelayOldPIE == 0) { m_dropProb *= 0.98; }
  if (m_dropProb < 0) { m_dropProb = 0; }
  if (m_dropProb > 1) { m_dropProb = 1; }

  m_qDelayOldPIE = qDelay;
  
  m_burstAllowancePIE = std::max(0.0, m_burstAllowancePIE - 0.03);
  
  m_rtrsEventPIE = Simulator::Schedule (Time (Seconds (m_tUpdatePIE)), &LteRlcUm::CalculateP, this);
}

void
LteRlcUm::ThroughputTrace (uint64_t bytesInQueueL4S, uint64_t bytesInQueueClassic)
{
  uint32_t bytesInBufferL4S = this->m_txBufferSize;
  uint32_t bytesTransmittedL4S = bytesInQueueL4S + m_throughputCount - bytesInBufferL4S;
  this->m_throughputCount = 0;
  
  uint32_t bytesInBufferClassic = 0;
  uint32_t bytesTransmittedClassic = 0;
  if (m_qIsInit)
    {
	  bytesInBufferClassic = m_queueHandle->m_txBufferSize;
	  bytesTransmittedClassic = bytesInQueueClassic + m_queueHandle->m_throughputCount - bytesInBufferClassic;
	  m_queueHandle->m_throughputCount = 0;
	}
  
  (*m_throughputFile) << (int) Simulator::Now ().GetSeconds () << "," << bytesTransmittedL4S / 125000.0 << "," << bytesTransmittedClassic / 125000.0 << "\n";
  
  //std::cout << Simulator::Now ().GetSeconds () << "s : " << m_txBuffer.size () << " " << m_queueHandle->m_txBuffer.size () << "\n";
  
  m_throughputTraceEvent = Simulator::Schedule (Time (Seconds (1)), &LteRlcUm::ThroughputTrace, this, bytesInBufferL4S, bytesInBufferClassic);
}

bool
LteRlcUm::DropEarlyPIE (Ptr<Packet> p)
{
  // turn off AQM
  //return false;
  
  double u = m_uv->GetValue ();
  
  // drop early for coupled
  if (m_l4s)
    {
	  if ((uint32_t) m_lcid == 5)
		{
		  if ((GetQueueDelay (this) > 0.001) && (m_txBufferSize > 3004)) { return true; }
		  if (sqrt (m_queueHandle->m_dropProb) * m_queueHandle->m_k > u) { return true; }
		}
	  else if ((uint32_t) m_lcid == 4)
		{
		  if ((m_qDelayOldPIE < 0.5 * m_qDelayRefPIE) && (m_dropProb < 0.2)) { return false; }
		  if (m_txBuffer.size () <= 2) { return false; }
		  
		  if (m_dropProb > u) { return true; }
		}
	  return false;
    }
  // drop early for uncoupled
  else
    {
	  if (m_qDelayOldPIE < 0.5 * m_qDelayRefPIE && m_dropProb < 0.2) { return false;}
	  if (m_txBuffer.size () <= 2) { return false; }
	  
	  if ( m_dropProb > u ) { return true; }
	  return false;
	}
}

bool
LteRlcUm::DropEarlyRED ()
{
  uint32_t length = m_txBuffer.size ();
  
  if (length <= m_minThRED) { return false; }
  if (length > m_maxThRED) { return true; }
  
  double u = m_uv->GetValue ();
  if ((double (length - m_minThRED) / double (m_maxThRED - m_minThRED) * m_maxProbRED) > u) { return true; }
  
  return false;
}

void
LteRlcUm::DoTransmitPdcpPdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << p->GetSize ());
  
  double now = Simulator::Now ().GetSeconds ();
  
  LtePdcpHeader pdcpHeader;
  Ipv4Header ipv4Header;
  p->RemoveHeader (pdcpHeader);
  p->PeekHeader (ipv4Header);	  
  p->AddHeader (pdcpHeader);
  
  if (!(ipv4Header.GetSource () == (Ipv4Address ("7.0.0.2")))) // only downstream
	{
	  if ((uint32_t) m_lcid == 5) // handle l4s
		{
		  if (!m_fIsInit) // if file not init, do init
			{
			  std::cout << now << " s -> L4S File Init\n";
			  
			  m_qDelayFile->open ("qDelayL4S");
			  m_actionsFile->open ("actionsL4S");
			  m_throughputFile->open ("throughput");
			  
			  (*m_throughputFile) << "2,0,0\n";
			  
			  // define AQM Parameters for l4s queue through command line input
			  bool scalable = false;
			  int nLF = 1;
			  int nCF = 1;
			  
			  std::string type;
			  std::cout << "Type:\n";
			  std::cin >> type;
			  
			  uint32_t found = type.find_first_of(",");
			  if (type.substr(0, found).compare ("0") == 0) { m_l4s = false; } // use l4s (coupling) or not
			  else { m_l4s = true; }
			  type = type.substr(found + 1);
			  
			  found = type.find_first_of(",");
			  if (type.substr(0, found).compare ("0") == 0) { m_red = false; } // use RED (else PIE)
			  else { m_red = true; }
			  type = type.substr(found + 1);
			  
			  found = type.find_first_of(",");
			  if (type.substr(0, found).compare ("0") == 0) { scalable = false; } // is scalable congestion control
			  else { scalable = true; }
			  type = type.substr(found + 1);
			  
			  found = type.find_first_of(",");
			  nLF = std::stoi (type.substr(0, found));
			  type = type.substr(found + 1);
			  
			  found = type.find_first_of(",");
			  nCF = std::stoi (type.substr(0, found));
			  type = type.substr(found + 1);
			  
			  double qDelayRef = std::stod(type);
			  
			  if (!m_l4s)
			    {
				  if (m_red)
				    {
					  double max = 172.0;
					  double share = 1.0;
					  if (nLF > 0) { share = double (nLF) / double (nCF + nLF); }
					  if (scalable)
					    {
						  if (qDelayRef == 0.001) { max = 2.0; }
						  else if (qDelayRef == 0.005) { max = 11.0; }
						  else if (qDelayRef == 0.015) { max = 24.0; }
						}
					  else
					    {
						  if (qDelayRef == 0.001) { max = 3.0; }
						  else if (qDelayRef == 0.005) { max = 27.0; }
						}
					  max = max * share;
					  double min = round (max / 3.0);
					  max = std::max (round (max), 1.0);
					  min = std::min (min, max - 1.0);
					  m_maxThRED = (uint32_t) max;
					  m_minThRED = (uint32_t) min;
					  std::cout << "DQ-RED - " << m_minThRED << " " << m_maxThRED << "\n";
					}
				  else
				    {
					  m_qDelayRefPIE = qDelayRef;
					  std::cout << "DQ-PIE - " << m_qDelayRefPIE << "ms\n";
					}
				}
			  else
			    {
				  std::cout << "L4S\n";
				}
			  
			  m_rtrsEventPIE = Simulator::Schedule (Time (Seconds (0.0)), &LteRlcUm::CalculateP, this);
			  m_throughputTraceEvent = Simulator::Schedule (Time (Seconds (0.95)), &LteRlcUm::ThroughputTrace, this, 0, 0);
			  m_fIsInit = true;
			}
		  if (!m_qIsInit) // if queue not init, do init
			{
			  std::cout << now << " s -> L4S Queue Init - ";
			  
			  // decrease queue size
			  //m_maxTxBufferSize = 2000;
			  
			  m_secToken = 1198731686;
			  m_queueHandle = this;
			  m_queueHandle.Decrease (2128);
			  if (m_queueHandle->m_secToken == m_secToken) // check security token
				{
				  std::cout << "Security Token does match\n";
				  m_qIsInit = true;
				}
			  else
				{
				  std::cout << "Security Token does not match " << this << "\n";
				  m_queueHandle = this;
				}
			}
		  if (!m_l4s)
		    {
			  if (m_dropProb == 0 && GetQueueDelay (this) < 0.5 * m_qDelayRefPIE && m_qDelayOldPIE < m_qDelayRefPIE)
				{
				  m_burstAllowancePIE = m_maxBurstAllowancePIE;
			    }
			}

		  if ((DropEarlyPIE (p) && m_l4s && !m_red) || (!m_red && DropEarlyPIE (p) && !m_l4s && (m_burstAllowancePIE == 0)) || (m_red && DropEarlyRED ())) // Mark or Drop
			{
			  if (ipv4Header.GetEcn () == ns3::Ipv4Header::EcnType::ECN_NotECT)
			    {
				  (*m_actionsFile) << now << ",D\n";
				  return;
				}
			  p->RemoveHeader (pdcpHeader);
			  p->RemoveHeader (ipv4Header);
			  ipv4Header.SetEcn (ns3::Ipv4Header::EcnType::ECN_CE);
			  ipv4Header.EnableChecksum ();
			  p->AddHeader (ipv4Header);
			  p->AddHeader (pdcpHeader);
			  
			  (*m_actionsFile) << now << ",M\n";
			}
		    // count enqueue of L4S traffic
		    if (ipv4Header.GetSource () == Ipv4Address ("1.0.0.1")) { this->m_throughputCount += p->GetSize (); }
			else if (m_qIsInit) { m_queueHandle->m_throughputCount += p->GetSize (); }
		}
	  if ((uint32_t) m_lcid == 4) // handle classic
		{
		  if (!m_fIsInit) // if file not init, do init
			{
			  std::cout << now << " s -> Classic File Init\n";
			  
			  m_qDelayFile->open ("qDelayClassic");
			  m_actionsFile->open ("actionsClassic");
			  
			  // only schedule CalculateP, file opened by l4s bearer
			  m_rtrsEventPIE = Simulator::Schedule (Time (Seconds (0.0)), &LteRlcUm::CalculateP, this);
			  
			  m_fIsInit = true;
			}
		  if (!m_qIsInit) // if queue not init, do init
			{
			  std::cout << now << " s -> Classic Queue Init - ";
			  
			  // decrease queue size
			  //m_maxTxBufferSize = 2000;
			  
			  m_secToken = 1198731686;
			  m_queueHandle = this;
			  m_queueHandle.Decrease (-2128);
			  if ((m_queueHandle->m_secToken == m_secToken) && (m_queueHandle->m_fIsInit)) // check security token
				{
				  std::cout << "Security Token does match\n";
				  
				  // define AQM Parameters for classic queue
				  m_l4s = m_queueHandle->m_l4s;
				  m_red = m_queueHandle->m_red;
				  if (!m_l4s)
					{
					  m_qDelayRefPIE = 0.5;
					  m_minThRED = 100;
					  m_maxThRED = 300;
					}
				  
				  m_qIsInit = true;
				}
			  else
				{
				  std::cout << "Security Token does not match " << this << "\n";
				  m_queueHandle = this;
				}
			}
		  if (m_dropProb == 0 && GetQueueDelay (this) < 0.5 * m_qDelayRefPIE && m_qDelayOldPIE < m_qDelayRefPIE)
		    {
			  m_burstAllowancePIE = m_maxBurstAllowancePIE;
			}
		  if ((!m_red && DropEarlyPIE (p) && m_burstAllowancePIE == 0) || (m_red && DropEarlyRED ())) // Mark or Drop
			{
			  if (ipv4Header.GetEcn () == ns3::Ipv4Header::EcnType::ECN_NotECT)
				{
				  (*m_actionsFile) << now << ",D\n";
				  return;
				}
			  p->RemoveHeader (pdcpHeader);
			  p->RemoveHeader (ipv4Header);
			  ipv4Header.SetEcn (ns3::Ipv4Header::EcnType::ECN_CE);
			  ipv4Header.EnableChecksum ();
			  p->AddHeader (ipv4Header);
			  p->AddHeader (pdcpHeader);
			  
			  (*m_actionsFile) << now << ",M\n";
			}
		  // count enqueue of L4S traffic
		  if (ipv4Header.GetSource () == Ipv4Address ("2.0.0.1")) { this->m_throughputCount += p->GetSize (); }
		  else if (m_qIsInit) { m_queueHandle->m_throughputCount += p->GetSize (); }
		}
	}

  if (m_txBufferSize + p->GetSize () <= m_maxTxBufferSize)
    {
      // Increase Bytes Counter
      m_enqueuedBytes += p->GetSize ();
      
      /** Store arrival time */
      RlcTag timeTag (Simulator::Now ());
      p->AddPacketTag (timeTag);

      /** Store PDCP PDU */

      LteRlcSduStatusTag tag;
      tag.SetStatus (LteRlcSduStatusTag::FULL_SDU);
      p->AddPacketTag (tag);

      NS_LOG_LOGIC ("Tx Buffer: New packet added");
      m_txBuffer.push_back (p);
      m_txBufferSize += p->GetSize ();
      NS_LOG_LOGIC ("NumOfBuffers = " << m_txBuffer.size() );
      NS_LOG_LOGIC ("txBufferSize = " << m_txBufferSize);
    }
  else
    {
      std::cout << now << "FORCED DROP\n";
      (*m_actionsFile) << now << ",F\n";
      
      // Discard full RLC SDU
      NS_LOG_LOGIC ("TxBuffer is full. RLC SDU discarded");
      NS_LOG_LOGIC ("MaxTxBufferSize = " << m_maxTxBufferSize);
      NS_LOG_LOGIC ("txBufferSize    = " << m_txBufferSize);
      NS_LOG_LOGIC ("packet size     = " << p->GetSize ());
    }

  /** Report Buffer Status */
  DoReportBufferStatus ();
  m_rbsTimer.Cancel ();
}


/**
 * MAC SAP
 */

void
LteRlcUm::DoNotifyTxOpportunity (uint32_t bytes, uint8_t layer, uint8_t harqId)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << bytes);

  double now = Simulator::Now ().GetSeconds ();
  std::string qDelayString;
  
  if (bytes <= 2)
    {
      // Stingy MAC: Header fix part is 2 bytes, we need more bytes for the data
      NS_LOG_LOGIC ("TX opportunity too small = " << bytes);
      return;
    }

  Ptr<Packet> packet = Create<Packet> ();
  LteRlcHeader rlcHeader;

  // Build Data field
  uint32_t nextSegmentSize = bytes - 2;
  uint32_t nextSegmentId = 1;
  uint32_t dataFieldTotalSize = 0;
  uint32_t dataFieldAddedSize = 0;
  std::vector < Ptr<Packet> > dataField;

  // Remove the first packet from the transmission buffer.
  // If only a segment of the packet is taken, then the remaining is given back later
  if ( m_txBuffer.size () == 0 )
    {
      NS_LOG_LOGIC ("No data pending");
      return;
    }

  NS_LOG_LOGIC ("SDUs in TxBuffer  = " << m_txBuffer.size ());
  NS_LOG_LOGIC ("First SDU buffer  = " << *(m_txBuffer.begin()));
  NS_LOG_LOGIC ("First SDU size    = " << (*(m_txBuffer.begin()))->GetSize ());
  NS_LOG_LOGIC ("Next segment size = " << nextSegmentSize);
  NS_LOG_LOGIC ("Remove SDU from TxBuffer");
  Ptr<Packet> firstSegment = (*(m_txBuffer.begin ()))->Copy ();
  m_txBufferSize -= (*(m_txBuffer.begin()))->GetSize ();
  NS_LOG_LOGIC ("txBufferSize      = " << m_txBufferSize );
  m_txBuffer.erase (m_txBuffer.begin ());
  
  // add packet delay to delaystring
  qDelayString.append (std::to_string(now));
  qDelayString.append (",");
  qDelayString.append (std::to_string(now - GetTimeStamp (firstSegment)));
  qDelayString.append (",");
  qDelayString.append (std::to_string(m_txBuffer.size ()));

  while ( firstSegment && (firstSegment->GetSize () > 0) && (nextSegmentSize > 0) )
    {
      NS_LOG_LOGIC ("WHILE ( firstSegment && firstSegment->GetSize > 0 && nextSegmentSize > 0 )");
      NS_LOG_LOGIC ("    firstSegment size = " << firstSegment->GetSize ());
      NS_LOG_LOGIC ("    nextSegmentSize   = " << nextSegmentSize);
      if ( (firstSegment->GetSize () > nextSegmentSize) ||
           // Segment larger than 2047 octets can only be mapped to the end of the Data field
           (firstSegment->GetSize () > 2047)
         )
        {
          // Take the minimum size, due to the 2047-bytes 3GPP exception
          // This exception is due to the length of the LI field (just 11 bits)
          uint32_t currSegmentSize = std::min (firstSegment->GetSize (), nextSegmentSize);

          NS_LOG_LOGIC ("    IF ( firstSegment > nextSegmentSize ||");
          NS_LOG_LOGIC ("         firstSegment > 2047 )");

          // Segment txBuffer.FirstBuffer and
          // Give back the remaining segment to the transmission buffer
          Ptr<Packet> newSegment = firstSegment->CreateFragment (0, currSegmentSize);
          NS_LOG_LOGIC ("    newSegment size   = " << newSegment->GetSize ());

          // Status tag of the new and remaining segments
          // Note: This is the only place where a PDU is segmented and
          // therefore its status can change
          LteRlcSduStatusTag oldTag, newTag;
          firstSegment->RemovePacketTag (oldTag);
          newSegment->RemovePacketTag (newTag);
          if (oldTag.GetStatus () == LteRlcSduStatusTag::FULL_SDU)
            {
              newTag.SetStatus (LteRlcSduStatusTag::FIRST_SEGMENT);
              oldTag.SetStatus (LteRlcSduStatusTag::LAST_SEGMENT);
            }
          else if (oldTag.GetStatus () == LteRlcSduStatusTag::LAST_SEGMENT)
            {
              newTag.SetStatus (LteRlcSduStatusTag::MIDDLE_SEGMENT);
              //oldTag.SetStatus (LteRlcSduStatusTag::LAST_SEGMENT);
            }

          // Give back the remaining segment to the transmission buffer
          firstSegment->RemoveAtStart (currSegmentSize);
          NS_LOG_LOGIC ("    firstSegment size (after RemoveAtStart) = " << firstSegment->GetSize ());
          if (firstSegment->GetSize () > 0)
            {
              firstSegment->AddPacketTag (oldTag);

              m_txBuffer.insert (m_txBuffer.begin (), firstSegment);
              m_txBufferSize += (*(m_txBuffer.begin()))->GetSize ();
              
              // delete last line
              std::size_t found = qDelayString.find_last_of("\n");
			  qDelayString = qDelayString.substr (0, found);

              NS_LOG_LOGIC ("    TX buffer: Give back the remaining segment");
              NS_LOG_LOGIC ("    TX buffers = " << m_txBuffer.size ());
              NS_LOG_LOGIC ("    Front buffer size = " << (*(m_txBuffer.begin()))->GetSize ());
              NS_LOG_LOGIC ("    txBufferSize = " << m_txBufferSize );
            }
          else
            {
              // Whole segment was taken, so adjust tag
              if (newTag.GetStatus () == LteRlcSduStatusTag::FIRST_SEGMENT)
                {
                  newTag.SetStatus (LteRlcSduStatusTag::FULL_SDU);
                }
              else if (newTag.GetStatus () == LteRlcSduStatusTag::MIDDLE_SEGMENT)
                {
                  newTag.SetStatus (LteRlcSduStatusTag::LAST_SEGMENT);
                }
            }
          // Segment is completely taken or
          // the remaining segment is given back to the transmission buffer
          firstSegment = 0;

          // Put status tag once it has been adjusted
          newSegment->AddPacketTag (newTag);

          // Add Segment to Data field
          dataFieldAddedSize = newSegment->GetSize ();
          dataFieldTotalSize += dataFieldAddedSize;
          dataField.push_back (newSegment);
          newSegment = 0;

          // ExtensionBit (Next_Segment - 1) = 0
          rlcHeader.PushExtensionBit (LteRlcHeader::DATA_FIELD_FOLLOWS);

          // no LengthIndicator for the last one

          nextSegmentSize -= dataFieldAddedSize;
          nextSegmentId++;

          // nextSegmentSize MUST be zero (only if segment is smaller or equal to 2047)

          // (NO more segments) → exit
          // break;
        }
      else if ( (nextSegmentSize - firstSegment->GetSize () <= 2) || (m_txBuffer.size () == 0) )
        {
          NS_LOG_LOGIC ("    IF nextSegmentSize - firstSegment->GetSize () <= 2 || txBuffer.size == 0");
          // Add txBuffer.FirstBuffer to DataField
          dataFieldAddedSize = firstSegment->GetSize ();
          dataFieldTotalSize += dataFieldAddedSize;
          dataField.push_back (firstSegment);
          firstSegment = 0;

          // ExtensionBit (Next_Segment - 1) = 0
          rlcHeader.PushExtensionBit (LteRlcHeader::DATA_FIELD_FOLLOWS);

          // no LengthIndicator for the last one

          nextSegmentSize -= dataFieldAddedSize;
          nextSegmentId++;

          NS_LOG_LOGIC ("        SDUs in TxBuffer  = " << m_txBuffer.size ());
          if (m_txBuffer.size () > 0)
            {
              NS_LOG_LOGIC ("        First SDU buffer  = " << *(m_txBuffer.begin()));
              NS_LOG_LOGIC ("        First SDU size    = " << (*(m_txBuffer.begin()))->GetSize ());
            }
          NS_LOG_LOGIC ("        Next segment size = " << nextSegmentSize);

          // nextSegmentSize <= 2 (only if txBuffer is not empty)

          // (NO more segments) → exit
          // break;
        }
      else // (firstSegment->GetSize () < m_nextSegmentSize) && (m_txBuffer.size () > 0)
        {
          NS_LOG_LOGIC ("    IF firstSegment < NextSegmentSize && txBuffer.size > 0");
          // Add txBuffer.FirstBuffer to DataField
          dataFieldAddedSize = firstSegment->GetSize ();
          dataFieldTotalSize += dataFieldAddedSize;
          dataField.push_back (firstSegment);

          // ExtensionBit (Next_Segment - 1) = 1
          rlcHeader.PushExtensionBit (LteRlcHeader::E_LI_FIELDS_FOLLOWS);

          // LengthIndicator (Next_Segment)  = txBuffer.FirstBuffer.length()
          rlcHeader.PushLengthIndicator (firstSegment->GetSize ());

          nextSegmentSize -= ((nextSegmentId % 2) ? (2) : (1)) + dataFieldAddedSize;
          nextSegmentId++;

          NS_LOG_LOGIC ("        SDUs in TxBuffer  = " << m_txBuffer.size ());
          if (m_txBuffer.size () > 0)
            {
              NS_LOG_LOGIC ("        First SDU buffer  = " << *(m_txBuffer.begin()));
              NS_LOG_LOGIC ("        First SDU size    = " << (*(m_txBuffer.begin()))->GetSize ());
            }
          NS_LOG_LOGIC ("        Next segment size = " << nextSegmentSize);
          NS_LOG_LOGIC ("        Remove SDU from TxBuffer");

          // (more segments)
          firstSegment = (*(m_txBuffer.begin ()))->Copy ();
          m_txBufferSize -= (*(m_txBuffer.begin()))->GetSize ();
          m_txBuffer.erase (m_txBuffer.begin ());
          
          // add packet delay to delaystring
          qDelayString.append ("\n");
		  qDelayString.append (std::to_string(now));
		  qDelayString.append (",");
		  qDelayString.append (std::to_string(now - GetTimeStamp (firstSegment)));
		  qDelayString.append (",");
		  qDelayString.append (std::to_string(m_txBuffer.size ()));
          
          NS_LOG_LOGIC ("        txBufferSize = " << m_txBufferSize );
        }

    }

  // Build RLC header
  rlcHeader.SetSequenceNumber (m_sequenceNumber++);

  // Build RLC PDU with DataField and Header
  std::vector< Ptr<Packet> >::iterator it;
  it = dataField.begin ();

  uint8_t framingInfo = 0;

  // FIRST SEGMENT
  LteRlcSduStatusTag tag;
  NS_ASSERT_MSG ((*it)->PeekPacketTag (tag), "LteRlcSduStatusTag is missing");
  (*it)->PeekPacketTag (tag);
  if ( (tag.GetStatus () == LteRlcSduStatusTag::FULL_SDU) ||
        (tag.GetStatus () == LteRlcSduStatusTag::FIRST_SEGMENT) )
    {
      framingInfo |= LteRlcHeader::FIRST_BYTE;
    }
  else
    {
      framingInfo |= LteRlcHeader::NO_FIRST_BYTE;
    }

  while (it < dataField.end ())
    {
      NS_LOG_LOGIC ("Adding SDU/segment to packet, length = " << (*it)->GetSize ());

      NS_ASSERT_MSG ((*it)->PeekPacketTag (tag), "LteRlcSduStatusTag is missing");
      (*it)->RemovePacketTag (tag);
      if (packet->GetSize () > 0)
        {
          packet->AddAtEnd (*it);
        }
      else
        {
          packet = (*it);
        }
      it++;
    }

  // LAST SEGMENT (Note: There could be only one and be the first one)
  it--;
  if ( (tag.GetStatus () == LteRlcSduStatusTag::FULL_SDU) ||
        (tag.GetStatus () == LteRlcSduStatusTag::LAST_SEGMENT) )
    {
      framingInfo |= LteRlcHeader::LAST_BYTE;
    }
  else
    {
      framingInfo |= LteRlcHeader::NO_LAST_BYTE;
    }

  rlcHeader.SetFramingInfo (framingInfo);

  NS_LOG_LOGIC ("RLC header: " << rlcHeader);
  packet->AddHeader (rlcHeader);

  // Sender timestamp
  RlcTag rlcTag (Simulator::Now ());
  packet->ReplacePacketTag (rlcTag);
  m_txPdu (m_rnti, m_lcid, packet->GetSize ());

  // Send RLC PDU to MAC layer
  LteMacSapProvider::TransmitPduParameters params;
  params.pdu = packet;
  params.rnti = m_rnti;
  params.lcid = m_lcid;
  params.layer = layer;
  params.harqProcessId = harqId;

  m_macSapProvider->TransmitPdu (params);
  
  // add qDelayString to file
  (*m_qDelayFile) << qDelayString << "\n";
  

  if (! m_txBuffer.empty ())
    {
      m_rbsTimer.Cancel ();
      m_rbsTimer = Simulator::Schedule (MilliSeconds (10), &LteRlcUm::ExpireRbsTimer, this);
    }
}

void
LteRlcUm::DoNotifyHarqDeliveryFailure ()
{
  NS_LOG_FUNCTION (this);
}

void
LteRlcUm::DoReceivePdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << p->GetSize ());

  // Receiver timestamp
  RlcTag rlcTag;
  Time delay;
  NS_ASSERT_MSG (p->PeekPacketTag (rlcTag), "RlcTag is missing");
  p->RemovePacketTag (rlcTag);
  delay = Simulator::Now() - rlcTag.GetSenderTimestamp ();
  m_rxPdu (m_rnti, m_lcid, p->GetSize (), delay.GetNanoSeconds ());

  // 5.1.2.2 Receive operations

  // Get RLC header parameters
  LteRlcHeader rlcHeader;
  p->PeekHeader (rlcHeader);
  NS_LOG_LOGIC ("RLC header: " << rlcHeader);
  SequenceNumber10 seqNumber = rlcHeader.GetSequenceNumber ();

  // 5.1.2.2.1 General
  // The receiving UM RLC entity shall maintain a reordering window according to state variable VR(UH) as follows:
  // - a SN falls within the reordering window if (VR(UH) - UM_Window_Size) <= SN < VR(UH);
  // - a SN falls outside of the reordering window otherwise.
  // When receiving an UMD PDU from lower layer, the receiving UM RLC entity shall:
  // - either discard the received UMD PDU or place it in the reception buffer (see sub clause 5.1.2.2.2);
  // - if the received UMD PDU was placed in the reception buffer:
  // - update state variables, reassemble and deliver RLC SDUs to upper layer and start/stop t-Reordering as needed (see sub clause 5.1.2.2.3);
  // When t-Reordering expires, the receiving UM RLC entity shall:
  // - update state variables, reassemble and deliver RLC SDUs to upper layer and start t-Reordering as needed (see sub clause 5.1.2.2.4).

  // 5.1.2.2.2 Actions when an UMD PDU is received from lower layer
  // When an UMD PDU with SN = x is received from lower layer, the receiving UM RLC entity shall:
  // - if VR(UR) < x < VR(UH) and the UMD PDU with SN = x has been received before; or
  // - if (VR(UH) - UM_Window_Size) <= x < VR(UR):
  //    - discard the received UMD PDU;
  // - else:
  //    - place the received UMD PDU in the reception buffer.

  NS_LOG_LOGIC ("VR(UR) = " << m_vrUr);
  NS_LOG_LOGIC ("VR(UX) = " << m_vrUx);
  NS_LOG_LOGIC ("VR(UH) = " << m_vrUh);
  NS_LOG_LOGIC ("SN = " << seqNumber);

  m_vrUr.SetModulusBase (m_vrUh - m_windowSize);
  m_vrUh.SetModulusBase (m_vrUh - m_windowSize);
  seqNumber.SetModulusBase (m_vrUh - m_windowSize);

  if ( ( (m_vrUr < seqNumber) && (seqNumber < m_vrUh) && (m_rxBuffer.count (seqNumber.GetValue ()) > 0) ) ||
       ( ((m_vrUh - m_windowSize) <= seqNumber) && (seqNumber < m_vrUr) )
     )
    {
      NS_LOG_LOGIC ("PDU discarded");
      p = 0;
      return;
    }
  else
    {
      NS_LOG_LOGIC ("Place PDU in the reception buffer");
      m_rxBuffer[seqNumber.GetValue ()] = p;
    }


  // 5.1.2.2.3 Actions when an UMD PDU is placed in the reception buffer
  // When an UMD PDU with SN = x is placed in the reception buffer, the receiving UM RLC entity shall:

  // - if x falls outside of the reordering window:
  //    - update VR(UH) to x + 1;
  //    - reassemble RLC SDUs from any UMD PDUs with SN that falls outside of the reordering window, remove
  //      RLC headers when doing so and deliver the reassembled RLC SDUs to upper layer in ascending order of the
  //      RLC SN if not delivered before;
  //    - if VR(UR) falls outside of the reordering window:
  //        - set VR(UR) to (VR(UH) - UM_Window_Size);

  if ( ! IsInsideReorderingWindow (seqNumber))
    {
      NS_LOG_LOGIC ("SN is outside the reordering window");

      m_vrUh = seqNumber + 1;
      NS_LOG_LOGIC ("New VR(UH) = " << m_vrUh);

      ReassembleOutsideWindow ();

      if ( ! IsInsideReorderingWindow (m_vrUr) )
        {
          m_vrUr = m_vrUh - m_windowSize;
          NS_LOG_LOGIC ("VR(UR) is outside the reordering window");
          NS_LOG_LOGIC ("New VR(UR) = " << m_vrUr);
        }
    }

  // - if the reception buffer contains an UMD PDU with SN = VR(UR):
  //    - update VR(UR) to the SN of the first UMD PDU with SN > current VR(UR) that has not been received;
  //    - reassemble RLC SDUs from any UMD PDUs with SN < updated VR(UR), remove RLC headers when doing
  //      so and deliver the reassembled RLC SDUs to upper layer in ascending order of the RLC SN if not delivered
  //      before;

  if ( m_rxBuffer.count (m_vrUr.GetValue ()) > 0 )
    {
      NS_LOG_LOGIC ("Reception buffer contains SN = " << m_vrUr);

      std::map <uint16_t, Ptr<Packet> >::iterator it;
      uint16_t newVrUr;
      SequenceNumber10 oldVrUr = m_vrUr;

      it = m_rxBuffer.find (m_vrUr.GetValue ());
      newVrUr = (it->first) + 1;
      while ( m_rxBuffer.count (newVrUr) > 0 )
        {
          newVrUr++;
        }
      m_vrUr = newVrUr;
      NS_LOG_LOGIC ("New VR(UR) = " << m_vrUr);

      ReassembleSnInterval (oldVrUr, m_vrUr);
    }

  // m_vrUh can change previously, set new modulus base
  // for the t-Reordering timer-related comparisons
  m_vrUr.SetModulusBase (m_vrUh - m_windowSize);
  m_vrUx.SetModulusBase (m_vrUh - m_windowSize);
  m_vrUh.SetModulusBase (m_vrUh - m_windowSize);

  // - if t-Reordering is running:
  //    - if VR(UX) <= VR(UR); or
  //    - if VR(UX) falls outside of the reordering window and VR(UX) is not equal to VR(UH)::
  //        - stop and reset t-Reordering;
  if ( m_reorderingTimer.IsRunning () )
    {
      NS_LOG_LOGIC ("Reordering timer is running");

      if ( (m_vrUx <= m_vrUr) ||
           ((! IsInsideReorderingWindow (m_vrUx)) && (m_vrUx != m_vrUh)) )
        {
          NS_LOG_LOGIC ("Stop reordering timer");
          m_reorderingTimer.Cancel ();
        }
    }

  // - if t-Reordering is not running (includes the case when t-Reordering is stopped due to actions above):
  //    - if VR(UH) > VR(UR):
  //        - start t-Reordering;
  //        - set VR(UX) to VR(UH).
  if ( ! m_reorderingTimer.IsRunning () )
    {
      NS_LOG_LOGIC ("Reordering timer is not running");

      if ( m_vrUh > m_vrUr )
        {
          NS_LOG_LOGIC ("VR(UH) > VR(UR)");
          NS_LOG_LOGIC ("Start reordering timer");
          m_reorderingTimer = Simulator::Schedule (Time ("0.1s"),
                                                   &LteRlcUm::ExpireReorderingTimer ,this);
          m_vrUx = m_vrUh;
          NS_LOG_LOGIC ("New VR(UX) = " << m_vrUx);
        }
    }

}


bool
LteRlcUm::IsInsideReorderingWindow (SequenceNumber10 seqNumber)
{
  NS_LOG_FUNCTION (this << seqNumber);
  NS_LOG_LOGIC ("Reordering Window: " <<
                m_vrUh << " - " << m_windowSize << " <= " << seqNumber << " < " << m_vrUh);

  m_vrUh.SetModulusBase (m_vrUh - m_windowSize);
  seqNumber.SetModulusBase (m_vrUh - m_windowSize);

  if ( ((m_vrUh - m_windowSize) <= seqNumber) && (seqNumber < m_vrUh))
    {
      NS_LOG_LOGIC (seqNumber << " is INSIDE the reordering window");
      return true;
    }
  else
    {
      NS_LOG_LOGIC (seqNumber << " is OUTSIDE the reordering window");
      return false;
    }
}


void
LteRlcUm::ReassembleAndDeliver (Ptr<Packet> packet)
{
  LteRlcHeader rlcHeader;
  packet->RemoveHeader (rlcHeader);
  uint8_t framingInfo = rlcHeader.GetFramingInfo ();
  SequenceNumber10 currSeqNumber = rlcHeader.GetSequenceNumber ();
  bool expectedSnLost;

  if ( currSeqNumber != m_expectedSeqNumber )
    {
      expectedSnLost = true;
      NS_LOG_LOGIC ("There are losses. Expected SN = " << m_expectedSeqNumber << ". Current SN = " << currSeqNumber);
      m_expectedSeqNumber = currSeqNumber + 1;
    }
  else
    {
      expectedSnLost = false;
      NS_LOG_LOGIC ("No losses. Expected SN = " << m_expectedSeqNumber << ". Current SN = " << currSeqNumber);
      m_expectedSeqNumber++;
    }

  // Build list of SDUs
  uint8_t extensionBit;
  uint16_t lengthIndicator;
  do
    {
      extensionBit = rlcHeader.PopExtensionBit ();
      NS_LOG_LOGIC ("E = " << (uint16_t)extensionBit);

      if ( extensionBit == 0 )
        {
          m_sdusBuffer.push_back (packet);
        }
      else // extensionBit == 1
        {
          lengthIndicator = rlcHeader.PopLengthIndicator ();
          NS_LOG_LOGIC ("LI = " << lengthIndicator);

          // Check if there is enough data in the packet
          if ( lengthIndicator >= packet->GetSize () )
            {
              NS_LOG_LOGIC ("INTERNAL ERROR: Not enough data in the packet (" << packet->GetSize () << "). Needed LI=" << lengthIndicator);
            }

          // Split packet in two fragments
          Ptr<Packet> data_field = packet->CreateFragment (0, lengthIndicator);
          packet->RemoveAtStart (lengthIndicator);

          m_sdusBuffer.push_back (data_field);
        }
    }
  while ( extensionBit == 1 );

  std::list < Ptr<Packet> >::iterator it;

  // Current reassembling state
  if      (m_reassemblingState == WAITING_S0_FULL)  NS_LOG_LOGIC ("Reassembling State = 'WAITING_S0_FULL'");
  else if (m_reassemblingState == WAITING_SI_SF)    NS_LOG_LOGIC ("Reassembling State = 'WAITING_SI_SF'");
  else                                              NS_LOG_LOGIC ("Reassembling State = Unknown state");

  // Received framing Info
  NS_LOG_LOGIC ("Framing Info = " << (uint16_t)framingInfo);

  // Reassemble the list of SDUs (when there is no losses)
  if (!expectedSnLost)
    {
      switch (m_reassemblingState)
        {
          case WAITING_S0_FULL:
                  switch (framingInfo)
                    {
                      case (LteRlcHeader::FIRST_BYTE | LteRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                              * Deliver one or multiple PDUs
                              */
                              for ( it = m_sdusBuffer.begin () ; it != m_sdusBuffer.end () ; it++ )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (*it);
                                }
                              m_sdusBuffer.clear ();
                      break;

                      case (LteRlcHeader::FIRST_BYTE | LteRlcHeader::NO_LAST_BYTE):
                              m_reassemblingState = WAITING_SI_SF;

                              /**
                              * Deliver full PDUs
                              */
                              while ( m_sdusBuffer.size () > 1 )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }

                              /**
                              * Keep S0
                              */
                              m_keepS0 = m_sdusBuffer.front ();
                              m_sdusBuffer.pop_front ();
                      break;

                      case (LteRlcHeader::NO_FIRST_BYTE | LteRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              /**
                               * Deliver zero, one or multiple PDUs
                               */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (LteRlcHeader::NO_FIRST_BYTE | LteRlcHeader::NO_LAST_BYTE):
                              if ( m_sdusBuffer.size () == 1 )
                                {
                                  m_reassemblingState = WAITING_S0_FULL;
                                }
                              else
                                {
                                  m_reassemblingState = WAITING_SI_SF;
                                }

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              if ( m_sdusBuffer.size () > 0 )
                                {
                                  /**
                                   * Deliver zero, one or multiple PDUs
                                   */
                                  while ( m_sdusBuffer.size () > 1 )
                                    {
                                      m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                      m_sdusBuffer.pop_front ();
                                    }

                                  /**
                                   * Keep S0
                                   */
                                  m_keepS0 = m_sdusBuffer.front ();
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      default:
                              /**
                              * ERROR: Transition not possible
                              */
                              NS_LOG_LOGIC ("INTERNAL ERROR: Transition not possible. FI = " << (uint32_t) framingInfo);
                      break;
                    }
          break;

          case WAITING_SI_SF:
                  switch (framingInfo)
                    {
                      case (LteRlcHeader::NO_FIRST_BYTE | LteRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                              * Deliver (Kept)S0 + SN
                              */
                              m_keepS0->AddAtEnd (m_sdusBuffer.front ());
                              m_sdusBuffer.pop_front ();
                              m_rlcSapUser->ReceivePdcpPdu (m_keepS0);

                              /**
                                * Deliver zero, one or multiple PDUs
                                */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (LteRlcHeader::NO_FIRST_BYTE | LteRlcHeader::NO_LAST_BYTE):
                              m_reassemblingState = WAITING_SI_SF;

                              /**
                              * Keep SI
                              */
                              if ( m_sdusBuffer.size () == 1 )
                                {
                                  m_keepS0->AddAtEnd (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                              else // m_sdusBuffer.size () > 1
                                {
                                  /**
                                  * Deliver (Kept)S0 + SN
                                  */
                                  m_keepS0->AddAtEnd (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                  m_rlcSapUser->ReceivePdcpPdu (m_keepS0);

                                  /**
                                  * Deliver zero, one or multiple PDUs
                                  */
                                  while ( m_sdusBuffer.size () > 1 )
                                    {
                                      m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                      m_sdusBuffer.pop_front ();
                                    }

                                  /**
                                  * Keep S0
                                  */
                                  m_keepS0 = m_sdusBuffer.front ();
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (LteRlcHeader::FIRST_BYTE | LteRlcHeader::LAST_BYTE):
                      case (LteRlcHeader::FIRST_BYTE | LteRlcHeader::NO_LAST_BYTE):
                      default:
                              /**
                                * ERROR: Transition not possible
                                */
                              NS_LOG_LOGIC ("INTERNAL ERROR: Transition not possible. FI = " << (uint32_t) framingInfo);
                      break;
                    }
          break;

          default:
                NS_LOG_LOGIC ("INTERNAL ERROR: Wrong reassembling state = " << (uint32_t) m_reassemblingState);
          break;
        }
    }
  else // Reassemble the list of SDUs (when there are losses, i.e. the received SN is not the expected one)
    {
      switch (m_reassemblingState)
        {
          case WAITING_S0_FULL:
                  switch (framingInfo)
                    {
                      case (LteRlcHeader::FIRST_BYTE | LteRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Deliver one or multiple PDUs
                               */
                              for ( it = m_sdusBuffer.begin () ; it != m_sdusBuffer.end () ; it++ )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (*it);
                                }
                              m_sdusBuffer.clear ();
                      break;

                      case (LteRlcHeader::FIRST_BYTE | LteRlcHeader::NO_LAST_BYTE):
                              m_reassemblingState = WAITING_SI_SF;

                              /**
                               * Deliver full PDUs
                               */
                              while ( m_sdusBuffer.size () > 1 )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }

                              /**
                               * Keep S0
                               */
                              m_keepS0 = m_sdusBuffer.front ();
                              m_sdusBuffer.pop_front ();
                      break;

                      case (LteRlcHeader::NO_FIRST_BYTE | LteRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Discard SN
                               */
                              m_sdusBuffer.pop_front ();

                              /**
                               * Deliver zero, one or multiple PDUs
                               */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (LteRlcHeader::NO_FIRST_BYTE | LteRlcHeader::NO_LAST_BYTE):
                              if ( m_sdusBuffer.size () == 1 )
                                {
                                  m_reassemblingState = WAITING_S0_FULL;
                                }
                              else
                                {
                                  m_reassemblingState = WAITING_SI_SF;
                                }

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              if ( m_sdusBuffer.size () > 0 )
                                {
                                  /**
                                  * Deliver zero, one or multiple PDUs
                                  */
                                  while ( m_sdusBuffer.size () > 1 )
                                    {
                                      m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                      m_sdusBuffer.pop_front ();
                                    }

                                  /**
                                  * Keep S0
                                  */
                                  m_keepS0 = m_sdusBuffer.front ();
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      default:
                              /**
                               * ERROR: Transition not possible
                               */
                              NS_LOG_LOGIC ("INTERNAL ERROR: Transition not possible. FI = " << (uint32_t) framingInfo);
                      break;
                    }
          break;

          case WAITING_SI_SF:
                  switch (framingInfo)
                    {
                      case (LteRlcHeader::FIRST_BYTE | LteRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Discard S0
                               */
                              m_keepS0 = 0;

                              /**
                               * Deliver one or multiple PDUs
                               */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (LteRlcHeader::FIRST_BYTE | LteRlcHeader::NO_LAST_BYTE):
                              m_reassemblingState = WAITING_SI_SF;

                              /**
                               * Discard S0
                               */
                              m_keepS0 = 0;

                              /**
                               * Deliver zero, one or multiple PDUs
                               */
                              while ( m_sdusBuffer.size () > 1 )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }

                              /**
                               * Keep S0
                               */
                              m_keepS0 = m_sdusBuffer.front ();
                              m_sdusBuffer.pop_front ();

                      break;

                      case (LteRlcHeader::NO_FIRST_BYTE | LteRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Discard S0
                               */
                              m_keepS0 = 0;

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              /**
                               * Deliver zero, one or multiple PDUs
                               */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (LteRlcHeader::NO_FIRST_BYTE | LteRlcHeader::NO_LAST_BYTE):
                              if ( m_sdusBuffer.size () == 1 )
                                {
                                  m_reassemblingState = WAITING_S0_FULL;
                                }
                              else
                                {
                                  m_reassemblingState = WAITING_SI_SF;
                                }

                              /**
                               * Discard S0
                               */
                              m_keepS0 = 0;

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              if ( m_sdusBuffer.size () > 0 )
                                {
                                  /**
                                   * Deliver zero, one or multiple PDUs
                                   */
                                  while ( m_sdusBuffer.size () > 1 )
                                    {
                                      m_rlcSapUser->ReceivePdcpPdu (m_sdusBuffer.front ());
                                      m_sdusBuffer.pop_front ();
                                    }

                                  /**
                                   * Keep S0
                                   */
                                  m_keepS0 = m_sdusBuffer.front ();
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      default:
                              /**
                                * ERROR: Transition not possible
                                */
                              NS_LOG_LOGIC ("INTERNAL ERROR: Transition not possible. FI = " << (uint32_t) framingInfo);
                      break;
                    }
          break;

          default:
                NS_LOG_LOGIC ("INTERNAL ERROR: Wrong reassembling state = " << (uint32_t) m_reassemblingState);
          break;
        }
    }

}


void
LteRlcUm::ReassembleOutsideWindow (void)
{
  NS_LOG_LOGIC ("Reassemble Outside Window");

  std::map <uint16_t, Ptr<Packet> >::iterator it;
  it = m_rxBuffer.begin ();

  while ( (it != m_rxBuffer.end ()) && ! IsInsideReorderingWindow (SequenceNumber10 (it->first)) )
    {
      NS_LOG_LOGIC ("SN = " << it->first);

      // Reassemble RLC SDUs and deliver the PDCP PDU to upper layer
      ReassembleAndDeliver (it->second);

      std::map <uint16_t, Ptr<Packet> >::iterator it_tmp = it;
      ++it;
      m_rxBuffer.erase (it_tmp);
    }

  if (it != m_rxBuffer.end ())
    {
      NS_LOG_LOGIC ("(SN = " << it->first << ") is inside the reordering window");
    }
}

void
LteRlcUm::ReassembleSnInterval (SequenceNumber10 lowSeqNumber, SequenceNumber10 highSeqNumber)
{
  NS_LOG_LOGIC ("Reassemble SN between " << lowSeqNumber << " and " << highSeqNumber);

  std::map <uint16_t, Ptr<Packet> >::iterator it;

  SequenceNumber10 reassembleSn = lowSeqNumber;
  NS_LOG_LOGIC ("reassembleSN = " << reassembleSn);
  NS_LOG_LOGIC ("highSeqNumber = " << highSeqNumber);
  while (reassembleSn < highSeqNumber)
    {
      NS_LOG_LOGIC ("reassembleSn < highSeqNumber");
      it = m_rxBuffer.find (reassembleSn.GetValue ());
      NS_LOG_LOGIC ("it->first  = " << it->first);
      NS_LOG_LOGIC ("it->second = " << it->second);
      if (it != m_rxBuffer.end () )
        {
          NS_LOG_LOGIC ("SN = " << it->first);

          // Reassemble RLC SDUs and deliver the PDCP PDU to upper layer
          ReassembleAndDeliver (it->second);

          m_rxBuffer.erase (it);
        }
        
      reassembleSn++;
    }
}


void
LteRlcUm::DoReportBufferStatus (void)
{
  Time holDelay (0);
  uint32_t queueSize = 0;

  if (! m_txBuffer.empty ())
    {
      RlcTag holTimeTag;
      NS_ASSERT_MSG (m_txBuffer.front ()->PeekPacketTag (holTimeTag), "RlcTag is missing");
      m_txBuffer.front ()->PeekPacketTag (holTimeTag);
      holDelay = Simulator::Now () - holTimeTag.GetSenderTimestamp ();

      queueSize = m_txBufferSize + 2 * m_txBuffer.size (); // Data in tx queue + estimated headers size
    }

  LteMacSapProvider::ReportBufferStatusParameters r;
  r.rnti = m_rnti;
  r.lcid = m_lcid;
  r.txQueueSize = queueSize;
  r.txQueueHolDelay = holDelay.GetMilliSeconds () ;
  r.retxQueueSize = 0;
  r.retxQueueHolDelay = 0;
  r.statusPduSize = 0;

  NS_LOG_LOGIC ("Send ReportBufferStatus = " << r.txQueueSize << ", " << r.txQueueHolDelay );
  m_macSapProvider->ReportBufferStatus (r);
}


void
LteRlcUm::ExpireReorderingTimer (void)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid);
  NS_LOG_LOGIC ("Reordering timer has expired");

  // 5.1.2.2.4 Actions when t-Reordering expires
  // When t-Reordering expires, the receiving UM RLC entity shall:
  // - update VR(UR) to the SN of the first UMD PDU with SN >= VR(UX) that has not been received;
  // - reassemble RLC SDUs from any UMD PDUs with SN < updated VR(UR), remove RLC headers when doing so
  //   and deliver the reassembled RLC SDUs to upper layer in ascending order of the RLC SN if not delivered before;
  // - if VR(UH) > VR(UR):
  //    - start t-Reordering;
  //    - set VR(UX) to VR(UH).

  std::map <uint16_t, Ptr<Packet> >::iterator it;
  SequenceNumber10 newVrUr = m_vrUx;

  while ( (it = m_rxBuffer.find (newVrUr.GetValue ())) != m_rxBuffer.end () )
    {
      newVrUr++;
    }
  SequenceNumber10 oldVrUr = m_vrUr;
  m_vrUr = newVrUr;
  NS_LOG_LOGIC ("New VR(UR) = " << m_vrUr);

  ReassembleSnInterval (oldVrUr, m_vrUr);

  if ( m_vrUh > m_vrUr)
    {
      NS_LOG_LOGIC ("Start reordering timer");
      m_reorderingTimer = Simulator::Schedule (Time ("0.1s"),
                                               &LteRlcUm::ExpireReorderingTimer, this);
      m_vrUx = m_vrUh;
      NS_LOG_LOGIC ("New VR(UX) = " << m_vrUx);
    }
}


void
LteRlcUm::ExpireRbsTimer (void)
{
  NS_LOG_LOGIC ("RBS Timer expires");

  if (! m_txBuffer.empty ())
    {
      DoReportBufferStatus ();
      m_rbsTimer = Simulator::Schedule (MilliSeconds (10), &LteRlcUm::ExpireRbsTimer, this);
    }
}

} // namespace ns3
