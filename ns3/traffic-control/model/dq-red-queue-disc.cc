/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 NITK Surathkal
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
 * Authors: Shravya Ks <shravya.ks0@gmail.com>
 *          Smriti Murali <m.smriti.95@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

/*
 * PORT NOTE: This code was ported from ns-2.36rc1 (queue/pie.cc).
 * Most of the comments are also ported from the same.
 */

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "dq-red-queue-disc.h"
#include "ns3/drop-tail-queue.h"

#include "ns3/ipv4-queue-disc-item.h"

#include <iostream>
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DQREDQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (DQREDQueueDisc);

TypeId DQREDQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DQREDQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<DQREDQueueDisc> ()
    .AddAttribute ("A",
                   "Value of alpha",
                   DoubleValue (0.125),
                   MakeDoubleAccessor (&DQREDQueueDisc::m_a),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("B",
                   "Value of beta",
                   DoubleValue (1.25),
                   MakeDoubleAccessor (&DQREDQueueDisc::m_b),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Tupdate",
                   "Time period to calculate drop probability",
                   TimeValue (Seconds (0.03)),
                   MakeTimeAccessor (&DQREDQueueDisc::m_tUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("Supdate",
                   "Start time of the update timer",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&DQREDQueueDisc::m_sUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("QueueLimit",
                   "Queue limit in bytes/packets",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&DQREDQueueDisc::m_queueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("QueueDelayReferenceL4S",
                   "Desired queue delay for L4S",
                   TimeValue (Seconds (0.015)),
                   MakeTimeAccessor (&DQREDQueueDisc::m_qDelayRefL4S),
                   MakeTimeChecker ())
    .AddAttribute ("QueueDelayReferenceClassic",
                   "Desired queue delay for Classic",
                   TimeValue (Seconds (1)),// keeps classic queue full -> l4s dequeue stable
                   MakeTimeAccessor (&DQREDQueueDisc::m_qDelayRefClassic),
                   MakeTimeChecker ())
    .AddAttribute ("MaxBurstAllowanceL4S",
                   "Current max burst allowance in seconds before random drop",
                   TimeValue (Seconds (0.15)),
                   MakeTimeAccessor (&DQREDQueueDisc::m_maxBurstL4S),
                   MakeTimeChecker ())
    .AddAttribute ("MaxBurstAllowanceClassic",
                   "Current max burst allowance in seconds before random drop",
                   TimeValue (Seconds (0.15)),
                   MakeTimeAccessor (&DQREDQueueDisc::m_maxBurstClassic),
                   MakeTimeChecker ())
    .AddAttribute ("K",
                   "K for BW division",
                   DoubleValue (2),
                   MakeDoubleAccessor (&DQREDQueueDisc::m_k),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxThL4S",
                   "Maximum Threshold L4S",
                   UintegerValue (80),
                   MakeUintegerAccessor (&DQREDQueueDisc::m_maxThL4S),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MinThL4S",
                   "Minimum Threshold L4S",
                   UintegerValue (27),
                   MakeUintegerAccessor (&DQREDQueueDisc::m_minThL4S),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxThClassic",
                   "Maximum Threshold Classic",
                   UintegerValue (300),
                   MakeUintegerAccessor (&DQREDQueueDisc::m_maxThClassic),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MinThClassic",
                   "Minimum Threshold Classic",
                   UintegerValue (100),
                   MakeUintegerAccessor (&DQREDQueueDisc::m_minThClassic),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("nLF",
                   "Number of L4S Flows",
                   UintegerValue (1),
                   MakeUintegerAccessor (&DQREDQueueDisc::m_nLF),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("nCF",
                   "Number of Classic Flows",
                   UintegerValue (1),
                   MakeUintegerAccessor (&DQREDQueueDisc::m_nCF),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxProbL4S",
                   "Maximum Drop Probability of L4S",
                   DoubleValue (0.02),
                   MakeDoubleAccessor (&DQREDQueueDisc::m_maxProbL4S),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxProbClassic",
                   "Maximum Drop Probability of Classic",
                   DoubleValue (0.02),
                   MakeDoubleAccessor (&DQREDQueueDisc::m_maxProbClassic),
                   MakeDoubleChecker<double> ())
  ;

  return tid;
}

DQREDQueueDisc::DQREDQueueDisc ()
  : QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();
  //m_rtrsEvent = Simulator::Schedule (m_sUpdate, &DQREDQueueDisc::CalculateP, this);
  
  m_qDelayFileL4S.open ("qDelayL4S");
  m_qDelayFileClassic.open ("qDelayClassic");
  m_actionsFileL4S.open ("actionsL4S");
  m_actionsFileClassic.open ("actionsClassic");
}

DQREDQueueDisc::~DQREDQueueDisc ()
{
  NS_LOG_FUNCTION (this);
  
  m_qDelayFileL4S.close ();
  m_qDelayFileClassic.close ();
  m_actionsFileL4S.close ();
  m_actionsFileClassic.close ();
}

void
DQREDQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  Simulator::Remove (m_rtrsEvent);
  QueueDisc::DoDispose ();
}

DQREDQueueDisc::Stats
DQREDQueueDisc::GetStats ()
{
  NS_LOG_FUNCTION (this);
  return m_stats;
}

bool
DQREDQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  
  double now = Simulator::Now ().GetSeconds ();
  
  // Distinguish Classic L4S
  int qDist = 1;
  ns3::Ipv4Header::EcnType ecn = DynamicCast<Ipv4QueueDiscItem> (item)->GetHeader ().GetEcn ();
  if (ecn == ns3::Ipv4Header::ECN_ECT1 || ecn == ns3::Ipv4Header::ECN_CE) { qDist = 0; }
  
  if ((GetInternalQueue (0)->GetNPackets () + GetInternalQueue (1)->GetNPackets ()) >= m_queueLimit)
	{
      std::cout << Simulator::Now ().GetSeconds () << " FORCED DROP!\n";
      
      if (qDist == 0) { m_actionsFileL4S << now << ",F\n"; }
      else { m_actionsFileClassic << now << ",F\n"; }
      
      // Drops due to queue limit: reactive
      Drop (item);
      m_stats.forcedDrop++;
      return false;
    }
  
  // Treat L4S traffic
  if (qDist == 0)
    {
	  if (DropEarly (item, true))
	    {
		  // try to mark, if unsuccessful -> drop
		  if (DynamicCast<Ipv4QueueDiscItem> (item)->Mark () == false)
			{
			  Drop (item);
			  m_stats.unforcedDrop++;
			  
			  m_actionsFileL4S << now << ",D\n";
			  
			  return false;
			}
		  else
			{
			  m_tsqL4S.push (now);
			  m_stats.classicMark++;
			  m_actionsFileL4S << now << ",M\n";
			}
		}
	  else
	    {
		  m_tsqL4S.push (now);
		}
	}
  // Treat Classic traffic
  else
    {
	  if (DropEarly (item, false))
	    {
		  // try to mark, if unsuccessful -> drop
		  if (DynamicCast<Ipv4QueueDiscItem> (item)->Mark () == false)
			{
			  Drop (item);
			  m_stats.unforcedDrop++;
			  
			  m_actionsFileClassic << now << ",D\n";
			  
			  return false;
			}
		  else
			{
			  m_tsqClassic.push (now);
			  m_stats.classicMark++;
			  m_actionsFileClassic << now << ",M\n";
			}
		}
	  else
	    {
		  m_tsqClassic.push (now);
		}
	}

  // No drop
  bool retval = GetInternalQueue (qDist)->Enqueue (item);

  NS_LOG_LOGIC ("\t packetsInQueue  " << GetInternalQueue (1)->GetNPackets ());

  return retval;
}

void
DQREDQueueDisc::InitializeParams (void)
{
  // Initially queue is empty so variables are initialize to zero except m_dqCount
  m_dropProbL4S = 0;
  m_dropProbClassic = 0;
  m_burstState = NO_BURST;
  m_qDelayOldL4S = Time (Seconds (0));
  m_qDelayOldClassic = Time (Seconds (0));
  m_stats.forcedDrop = 0;
  m_stats.unforcedDrop = 0;
  
  m_stats.l4sMark = 0;
  m_stats.classicMark = 0;
  
  m_throughputClassic = 0;
  m_throughputL4S = 0;
  
  // normalize
  uint32_t gcd = std::__gcd (m_nLF, m_nCF);
  m_nLF = m_nLF / gcd;
  m_nCF = m_nCF / gcd;
  
  m_lastDq = 0;
  m_dqCount = 0;
}

bool DQREDQueueDisc::DropEarly (Ptr<QueueDiscItem> item, bool ect1)
{
  NS_LOG_FUNCTION (this << item);
  
  double u = m_uv->GetValue ();
  
  // drop probabilty for L4S
  if (ect1)
    {
	  uint32_t length = GetInternalQueue (0)->GetNPackets ();
	  
	  if (length > m_maxThL4S) { return true; }
	  else if (length <= m_minThL4S) { return false; }
	  else if ((double (length - m_minThL4S) / double (m_maxThL4S - m_minThL4S) * m_maxProbL4S) > u) { return true; }
	  
	  return false;
	}
  // drop probability for Classic
  else
    {
	  uint32_t length = GetInternalQueue (1)->GetNPackets ();
	  
	  if (length > m_maxThClassic) { return true; }
	  else if (length <= m_minThClassic) { return false; }
	  else if ((double (length - m_minThClassic) / double (m_maxThClassic - m_minThClassic) * m_maxProbClassic) > u) { return true; }
	  
	  return false;
	}
}

Ptr<QueueDiscItem>
DQREDQueueDisc::DoDequeue ()
{
  NS_LOG_FUNCTION (this);
  
  double now = Simulator::Now ().GetSeconds ();
  
  // decide on dequeueing
  if (m_tsqL4S.empty () && m_tsqClassic.empty ()) // both empty
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }
  else if (m_tsqClassic.empty ()) // Classic empty
    {
	  if (m_lastDq == 0) { m_dqCounter++; }
	  else { m_lastDq = 0; m_dqCounter = 1; }  
	}
  else if (m_tsqL4S.empty ()) // L4S empty
    {
	  if (m_lastDq == 1) { m_dqCounter++; }
	  else { m_lastDq = 1; m_dqCounter = 1; }
	}
  else // both have
    {
	  if (m_lastDq == 0)
	    {
		  if (m_dqCounter >= m_nLF) { m_lastDq = 1; m_dqCounter = 1; } // too many L4S -> give to Classic
		  else { m_dqCounter++; } // dequeue L4S and count
		}
	  else if (m_lastDq == 1)
	    {
		  if (m_dqCounter >= m_nCF) { m_lastDq = 0; m_dqCounter = 1; } // too many Classic -> give to L4S
		  else { m_dqCounter++; } // dequeue Classic and count
		}
	  else
	    {
		  std::cout << "ERROR IN RR - RESTARTING CYCLE\n";
		  m_lastDq = 0;
		  m_dqCounter = 0;
		  return 0;
		}
	}
  
  // dequeue
  Ptr<QueueDiscItem> item;
  
  if (m_lastDq == 0)
    {
	  if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ())) != 0)
	    {
	      m_qDelayFileL4S << now << "," << now - m_tsqL4S.front () << "," << GetInternalQueue (0)->GetNPackets () << "\n";
	      m_tsqL4S.pop ();
	    }
	  else { std::cout << "FATAL ERROR IN QUEUEs\n"; }	  
	}
  else if (m_lastDq == 1)
    {
	  if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (1)->Dequeue ())) != 0)
	    {
	      m_qDelayFileClassic << now << "," << now - m_tsqClassic.front () << "," << GetInternalQueue (1)->GetNPackets () << "\n";
	      m_tsqClassic.pop ();
	    }
	  else { std::cout << "FATAL ERROR IN QUEUES\n"; }
	}
  else
    {
	  std::cout << "ERROR IN RR - RESTARTING CYCLE\n";
	  m_lastDq = 0;
	  m_dqCounter = 0;
	  return 0;
	}

  // count throughput
  if (DynamicCast<Ipv4QueueDiscItem> (item)->GetHeader ().GetSource () == Ipv4Address ("10.1.1.1"))
    {
	  m_throughputL4S += DynamicCast<Ipv4QueueDiscItem> (item)->GetPacketSize () + 2;
	}
  else if (DynamicCast<Ipv4QueueDiscItem> (item)->GetHeader ().GetSource () == Ipv4Address ("10.1.2.1"))
    {
	  m_throughputClassic += DynamicCast<Ipv4QueueDiscItem> (item)->GetPacketSize () + 2;
	}

  return item;
}

Ptr<const QueueDiscItem>
DQREDQueueDisc::DoPeek () const
{
  NS_LOG_FUNCTION (this);
  if (GetInternalQueue (1)->IsEmpty () && GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }
  
  Ptr<const QueueDiscItem> item;

  if ((item = StaticCast<const QueueDiscItem> (GetInternalQueue (0)->Peek ())) != 0) {}
  else if ((item = StaticCast<const QueueDiscItem> (GetInternalQueue (1)->Peek ())) != 0) {}

  NS_LOG_LOGIC ("Number packets " << GetInternalQueue (1)->GetNPackets ());
  NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (1)->GetNBytes ());

  return item;
}

bool
DQREDQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("DQREDQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("DQREDQueueDisc cannot have packet filters");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // create a DropTail queue
      Ptr<Queue> queue = CreateObjectWithAttributes<DropTailQueue> ("Mode", EnumValue (m_mode));
      if (m_mode == Queue::QUEUE_MODE_PACKETS)
        {
          queue->SetMaxPackets (m_queueLimit);
        }
      else
        {
          queue->SetMaxBytes (m_queueLimit);
        }
      AddInternalQueue (queue);
    }

  if (GetNInternalQueues () != 2)
    {
      NS_LOG_ERROR ("DQREDQueueDisc needs 2 internal queues");
      return false;
    }

  if (GetInternalQueue (1)->GetMode () != Queue::QUEUE_MODE_PACKETS || GetInternalQueue (0)->GetMode () != Queue::QUEUE_MODE_PACKETS)
    {
      NS_LOG_ERROR ("Queues must be in Packet Mode");
      return false;
    }

  if ((m_mode ==  Queue::QUEUE_MODE_PACKETS && GetInternalQueue (1)->GetMaxPackets () < m_queueLimit))
    {
      NS_LOG_ERROR ("The size of the internal queue is less than the queue disc limit");
      return false;
    }

  return true;
}

} //namespace ns3
