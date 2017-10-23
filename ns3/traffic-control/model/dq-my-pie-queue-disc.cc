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
#include "dq-my-pie-queue-disc.h"
#include "ns3/drop-tail-queue.h"

#include "ns3/ipv4-queue-disc-item.h"

#include <iostream>
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DQMyPieQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (DQMyPieQueueDisc);

TypeId DQMyPieQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DQMyPieQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<DQMyPieQueueDisc> ()
    .AddAttribute ("A",
                   "Value of alpha",
                   DoubleValue (0.125),
                   MakeDoubleAccessor (&DQMyPieQueueDisc::m_a),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("B",
                   "Value of beta",
                   DoubleValue (1.25),
                   MakeDoubleAccessor (&DQMyPieQueueDisc::m_b),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TupdateL4S",
                   "Time period to calculate drop probability for L4S",
                   TimeValue (Seconds (0.015)),
                   MakeTimeAccessor (&DQMyPieQueueDisc::m_tUpdateL4S),
                   MakeTimeChecker ())
    .AddAttribute ("TupdateClassic",
                   "Time period to calculate drop probability for Classic",
                   TimeValue (Seconds (0.015)),
                   MakeTimeAccessor (&DQMyPieQueueDisc::m_tUpdateClassic),
                   MakeTimeChecker ())
    .AddAttribute ("Supdate",
                   "Start time of the update timer",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&DQMyPieQueueDisc::m_sUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("QueueLimit",
                   "Queue limit in bytes/packets",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&DQMyPieQueueDisc::m_queueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("QueueDelayReferenceL4S",
                   "Desired queue delay for L4S",
                   TimeValue (Seconds (0.015)),
                   MakeTimeAccessor (&DQMyPieQueueDisc::m_qDelayRefL4S),
                   MakeTimeChecker ())
    .AddAttribute ("QueueDelayReferenceClassic",
                   "Desired queue delay for Classic",
                   TimeValue (Seconds (0.5)),// keeps classic queue full -> l4s dequeue stable
                   MakeTimeAccessor (&DQMyPieQueueDisc::m_qDelayRefClassic),
                   MakeTimeChecker ())
    .AddAttribute ("MaxBurstAllowanceL4S",
                   "Current max burst allowance in seconds before random drop",
                   TimeValue (Seconds (0.15)),
                   MakeTimeAccessor (&DQMyPieQueueDisc::m_maxBurstL4S),
                   MakeTimeChecker ())
    .AddAttribute ("MaxBurstAllowanceClassic",
                   "Current max burst allowance in seconds before random drop",
                   TimeValue (Seconds (0.15)),
                   MakeTimeAccessor (&DQMyPieQueueDisc::m_maxBurstClassic),
                   MakeTimeChecker ())
    .AddAttribute ("K",
                   "K for BW division",
                   DoubleValue (2),
                   MakeDoubleAccessor (&DQMyPieQueueDisc::m_k),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("nLF",
                   "Number of L4S Flows",
                   UintegerValue (1),
                   MakeUintegerAccessor (&DQMyPieQueueDisc::m_nLF),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("nCF",
                   "Number of Classic Flows",
                   UintegerValue (1),
                   MakeUintegerAccessor (&DQMyPieQueueDisc::m_nCF),
                   MakeUintegerChecker<uint32_t> ())
  ;

  return tid;
}

DQMyPieQueueDisc::DQMyPieQueueDisc ()
  : QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();
  m_rtrsEventL4S = Simulator::Schedule (m_sUpdate, &DQMyPieQueueDisc::CalculatePL4S, this);
  m_rtrsEventClassic = Simulator::Schedule (m_sUpdate, &DQMyPieQueueDisc::CalculatePClassic, this);
  
  m_qDelayFileL4S.open ("qDelayL4S");
  m_qDelayFileClassic.open ("qDelayClassic");
  m_actionsFileL4S.open ("actionsL4S");
  m_actionsFileClassic.open ("actionsClassic");
  m_probabilityFile.open ("probability");
}

DQMyPieQueueDisc::~DQMyPieQueueDisc ()
{
  NS_LOG_FUNCTION (this);
  
  m_qDelayFileL4S.close ();
  m_qDelayFileClassic.close ();
  m_actionsFileL4S.close ();
  m_actionsFileClassic.close ();
  m_probabilityFile.close ();
}

void
DQMyPieQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  Simulator::Remove (m_rtrsEventL4S);
  Simulator::Remove (m_rtrsEventClassic);
  QueueDisc::DoDispose ();
}

DQMyPieQueueDisc::Stats
DQMyPieQueueDisc::GetStats ()
{
  NS_LOG_FUNCTION (this);
  return m_stats;
}

bool
DQMyPieQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  
  double now = Simulator::Now ().GetSeconds ();
  double qDelayL4S = 0;
  double qDelayClassic = 0;
  if (!m_tsqL4S.empty ()) { qDelayL4S = now - m_tsqL4S.front (); }
  if (!m_tsqClassic.empty ()) { qDelayClassic = now - m_tsqClassic.front (); }
  
  // Distinguish Classic L4S
  int qDist = 1;
  ns3::Ipv4Header::EcnType ecn = DynamicCast<Ipv4QueueDiscItem> (item)->GetHeader ().GetEcn ();
  if (ecn == ns3::Ipv4Header::ECN_ECT1 || ecn == ns3::Ipv4Header::ECN_CE) { qDist = 0; }
  
  if ((GetInternalQueue (0)->GetNPackets () + GetInternalQueue (1)->GetNPackets ()) >= m_queueLimit)
	{
      std::cout << now << " FORCED DROP!\n";
      
      if (qDist == 0) { m_actionsFileL4S << now << ",F\n"; }
      else { m_actionsFileClassic << now << ",F\n"; }
      
      // Drops due to queue limit
      Drop (item);
      m_stats.forcedDrop++;
      return false;
    }
  
  // Treat L4S traffic
  if (qDist == 0)
    {
	  if (m_dropProbL4S == 0 && qDelayL4S < 0.5 * m_qDelayRefL4S.GetSeconds () && m_qDelayOldL4S.GetSeconds () < m_qDelayRefL4S.GetSeconds ())
	    {
		  m_burstAllowanceL4S = m_maxBurstL4S;
		}
	  if (m_burstAllowanceL4S == 0 && DropEarly (item, true))
	    {
		  // try to mark, if unsuccessful -> drop
		  if (DynamicCast<Ipv4QueueDiscItem> (item)->Mark () == false)
			{
			  Drop (item);
			  m_actionsFileL4S << now << ",D\n";
			  
			  return false;
			}
		  else
			{
			  m_tsqL4S.push (now);
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
	  if (m_dropProbClassic == 0 && qDelayClassic < 0.5 * m_qDelayRefClassic.GetSeconds () && m_qDelayOldClassic.GetSeconds () < m_qDelayRefClassic.GetSeconds ())
	    {
		  m_burstAllowanceClassic = m_maxBurstClassic;
		}
	  if (m_burstAllowanceClassic == 0 && DropEarly (item, false))
	    {
		  // try to mark, if unsuccessful -> drop
		  if (DynamicCast<Ipv4QueueDiscItem> (item)->Mark () == false)
			{
			  Drop (item);
			  m_actionsFileClassic << now << ",D\n";
			  
			  return false;
			}
		  else
			{
			  m_tsqClassic.push (now);
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
DQMyPieQueueDisc::InitializeParams (void)
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
  m_dqCounter = 0;
}

bool DQMyPieQueueDisc::DropEarly (Ptr<QueueDiscItem> item, bool ect1)
{
  NS_LOG_FUNCTION (this << item);
  
  double u = m_uv->GetValue ();
  
  // drop probabilty for L4S
  if (ect1)
    {
	  if (m_qDelayOldL4S < 0.5 * m_qDelayRefL4S && m_dropProbL4S < 0.2) { return false; }
	  if (GetInternalQueue (0)->GetNPackets () <= 2) { return false; }
	  if (m_dropProbL4S > u) { return true; }
	  else { return false; }
	}
  // drop probability for Classic
  else
    {
	  if (m_qDelayOldClassic < 0.5 * m_qDelayRefClassic && m_dropProbClassic < 0.2) { return false; }
	  if (GetInternalQueue (1)->GetNPackets () <= 2) { return false; }
	  if (m_dropProbClassic > u) { return true; }
	  else { return false; }
	}
}

void DQMyPieQueueDisc::CalculatePL4S ()
{
  NS_LOG_FUNCTION (this);
  
  double now = Simulator::Now ().GetSeconds ();
  double qDelayL4S = 0;
  if (!m_tsqL4S.empty ()) { qDelayL4S = now - m_tsqL4S.front (); }
  
  // Calculate Drop Probability for L4S
  double p = m_a * (qDelayL4S - m_qDelayRefL4S.GetSeconds ()) + m_b * (qDelayL4S - m_qDelayOldL4S.GetSeconds ());

  if (m_dropProbL4S < 0.000001) { p /= 2048; }
  else if (m_dropProbL4S < 0.00001) { p /= 512; }
  else if (m_dropProbL4S < 0.0001) { p /= 128; }
  else if (m_dropProbL4S < 0.001) { p /= 32; }
  else if (m_dropProbL4S < 0.01) { p /= 8; }
  else if (m_dropProbL4S < 0.1) { p /= 2; }

  m_dropProbL4S += p;
  
  if (qDelayL4S == 0 && m_qDelayOldL4S == 0) { m_dropProbL4S *= 0.98; }
  
  if (m_dropProbL4S < 0) { m_dropProbL4S = 0; }
  if (m_dropProbL4S > 1) { m_dropProbL4S = 1; }
  
  m_qDelayOldL4S = Time (Seconds (qDelayL4S));
  
  m_burstAllowanceL4S = std::max(Seconds (0), m_burstAllowanceL4S - m_tUpdateL4S);
  
  m_probabilityFile << now << "," << m_dropProbL4S << "," << m_dropProbClassic << "\n";
  
  // Schedule next Calculation
  m_rtrsEventL4S = Simulator::Schedule (m_tUpdateL4S, &DQMyPieQueueDisc::CalculatePL4S, this);
}

void DQMyPieQueueDisc::CalculatePClassic ()
{
  NS_LOG_FUNCTION (this);
  
  double now = Simulator::Now ().GetSeconds ();
  double qDelayClassic = 0;
  if (!m_tsqClassic.empty ()) { qDelayClassic = now - m_tsqClassic.front (); }
  
  // Calculate Drop Probability for Classic
  double p = m_a * (qDelayClassic - m_qDelayRefClassic.GetSeconds ()) + m_b * (qDelayClassic - m_qDelayOldClassic.GetSeconds ());

  if (m_dropProbClassic < 0.000001) { p /= 2048; }
  else if (m_dropProbClassic < 0.00001) { p /= 512; }
  else if (m_dropProbClassic < 0.0001) { p /= 128; }
  else if (m_dropProbClassic < 0.001) { p /= 32; }
  else if (m_dropProbClassic < 0.01) { p /= 8; }
  else if (m_dropProbClassic < 0.1) { p /= 2; }

  m_dropProbClassic += p;
  
  if (qDelayClassic == 0 && m_qDelayOldClassic == 0) { m_dropProbClassic *= 0.98; }
  
  if (m_dropProbClassic < 0) { m_dropProbClassic = 0; }
  if (m_dropProbClassic > 1) { m_dropProbClassic = 1; }
  
  m_qDelayOldClassic = Time (Seconds (qDelayClassic));
  
  m_burstAllowanceClassic = std::max(Seconds (0), m_burstAllowanceClassic - m_tUpdateClassic);
  
  // Schedule next Calculation
  m_rtrsEventClassic = Simulator::Schedule (m_tUpdateClassic, &DQMyPieQueueDisc::CalculatePClassic, this);
}

Ptr<QueueDiscItem>
DQMyPieQueueDisc::DoDequeue ()
{
  NS_LOG_FUNCTION (this);
  
  double now = Simulator::Now ().GetSeconds ();
  
  // decide on dequeueing
  if (m_tsqL4S.empty () && m_tsqClassic.empty ()) // both empty
    {
      NS_LOG_LOGIC ("Queues empty");
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
DQMyPieQueueDisc::DoPeek () const
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
DQMyPieQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("DQMyPieQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("DQMyPieQueueDisc cannot have packet filters");
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
      NS_LOG_ERROR ("DQMyPieQueueDisc needs 2 internal queues");
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
