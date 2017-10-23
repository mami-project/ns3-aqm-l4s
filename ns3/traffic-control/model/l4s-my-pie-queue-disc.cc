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
#include "l4s-my-pie-queue-disc.h"
#include "ns3/drop-tail-queue.h"

#include "ns3/ipv4-queue-disc-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("L4SMyPieQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (L4SMyPieQueueDisc);

TypeId L4SMyPieQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::L4SMyPieQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<L4SMyPieQueueDisc> ()
    .AddAttribute ("A",
                   "Value of alpha",
                   DoubleValue (0.125),
                   MakeDoubleAccessor (&L4SMyPieQueueDisc::m_a),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("B",
                   "Value of beta",
                   DoubleValue (1.25),
                   MakeDoubleAccessor (&L4SMyPieQueueDisc::m_b),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Tupdate",
                   "Time period to calculate drop probability",
                   TimeValue (Seconds (0.015)),
                   MakeTimeAccessor (&L4SMyPieQueueDisc::m_tUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("Supdate",
                   "Start time of the update timer",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&L4SMyPieQueueDisc::m_sUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("QueueLimit",
                   "Queue limit in bytes/packets",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&L4SMyPieQueueDisc::m_queueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("QueueDelayReference",
                   "Desired queue delay",
                   TimeValue (Seconds (0.015)),
                   MakeTimeAccessor (&L4SMyPieQueueDisc::m_qDelayRef),
                   MakeTimeChecker ())
    .AddAttribute ("MaxBurstAllowance",
                   "Current max burst allowance in seconds before random drop",
                   TimeValue (Seconds (0.15)),
                   MakeTimeAccessor (&L4SMyPieQueueDisc::m_maxBurst),
                   MakeTimeChecker ())
    .AddAttribute ("K",
                   "K for BW division",
                   DoubleValue (2),
                   MakeDoubleAccessor (&L4SMyPieQueueDisc::m_k),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("L4SThreshLen",
                   "L4SThreshold in Length",
                   UintegerValue (3004), // 2 * MTU
                   MakeUintegerAccessor (&L4SMyPIEQueueDisc::m_l4sThreshLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("L4SThreshTime",
                   "L4SThreshold in Time",
                   TimeValue (Seconds (0.001)),
                   MakeTimeAccessor (&L4SMyPIEQueueDisc::m_l4sThreshTime),
                   MakeTimeChecker ())
  ;

  return tid;
}

L4SMyPieQueueDisc::L4SMyPieQueueDisc ()
  : QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();
  m_rtrsEvent = Simulator::Schedule (m_sUpdate, &L4SMyPieQueueDisc::CalculateP, this);
  
  m_qDelayFileL4S.open ("qDelayL4S");
  m_qDelayFileClassic.open ("qDelayClassic");
  m_actionsFileL4S.open ("actionsL4S");
  m_actionsFileClassic.open ("actionsClassic");
}

L4SMyPieQueueDisc::~L4SMyPieQueueDisc ()
{
  NS_LOG_FUNCTION (this);
  
  m_qDelayFileL4S.close ();
  m_qDelayFileClassic.close ();
  m_actionsFileL4S.close ();
  m_actionsFileClassic.close ();
}

void
L4SMyPieQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  Simulator::Remove (m_rtrsEvent);
  QueueDisc::DoDispose ();
}

L4SMyPieQueueDisc::Stats
L4SMyPieQueueDisc::GetStats ()
{
  NS_LOG_FUNCTION (this);
  return m_stats;
}

bool
L4SMyPieQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  
  double now = Simulator::Now ().GetSeconds ();
  double qDelayClassic = 0;
  double qDelayL4S = 0;
  if (!m_tsqClassic.empty ()) { qDelayClassic = now - m_tsqClassic.front (); }
  if (!m_tsqL4S.empty ()) { qDelayL4S = now - m_tsqL4S.front (); }
  
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
  
  // Treat L4S Traffic
  if (qDist == 0)
    {
	  if (DropEarly (item, true, qDelayL4S))
	    {
		  if (DynamicCast<Ipv4QueueDiscItem> (item)->Mark () == false)
			{
			  Drop (item);
			  m_stats.unforcedDrop++;
			  
			  m_actionsFileL4S << now << ",D\n";
			  
			  return 0;
			}
		  else
			{
			  m_actionsFileL4S << now << ",M\n";
			  
			  m_tsqL4S.push (now);
			  m_stats.classicMark++;
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
	  if ((m_dropProb == 0) && (qDelayClassic < (0.5 * m_qDelayRef.GetSeconds ())) && (m_qDelayOld.GetSeconds () < (0.5 * m_qDelayRef.GetSeconds ())))
		{
		  m_burstAllowance = m_maxBurst;
		}
	  if (m_burstAllowance == 0 && DropEarly (item, false, 0.0))
	    {
		  if (DynamicCast<Ipv4QueueDiscItem> (item)->Mark () == false)
			{
			  Drop (item);
			  
			  m_actionsFileClassic << now << ",D\n";
			  
			  return 0;
			}
		  else
			{
			  m_actionsFileClassic << now << ",M\n";
			  
			  m_tsqClassic.push (now);
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
L4SMyPieQueueDisc::InitializeParams (void)
{
  // Initially queue is empty so variables are initialize to zero except m_dqCount
  m_dropProb = 0;
  m_burstState = NO_BURST;
  m_qDelayOld = Time (Seconds (0));
  m_stats.forcedDrop = 0;
  m_stats.unforcedDrop = 0;
  
  m_stats.l4sMark = 0;
  m_stats.classicMark = 0;
  
  m_throughputClassic = 0;
  m_throughputL4S = 0;
}

bool L4SMyPieQueueDisc::DropEarly (Ptr<QueueDiscItem> item, bool ect1, double qDelayL4S)
{
  NS_LOG_FUNCTION (this << item);
  
  double u = m_uv->GetValue ();
  
  // drop early for L4S
  if (ect1)
    {
	  if (sqrt (m_dropProb) * m_k > u) { return true; }
	  // mark packet if queue delay or size over threshold
	  if (qDelayL4S > m_l4sThreshTime.GetSeconds () && GetInternalQueue (0)->GetNBytes () > m_l4sThreshLen) { return true; }
	}
  // drop early for Classic
  else
    {
	  if (((m_qDelayOld < (0.5 * m_qDelayRef)) && (m_dropProb < 0.2))) { return false; }
	  if (GetInternalQueue (1)->GetNPackets () <= 2) { return false; }
	  
	  if (m_dropProb > u) { return true; }
  	}
  return false;
}

void L4SMyPieQueueDisc::CalculateP ()
{
  NS_LOG_FUNCTION (this);
  
  double now = Simulator::Now ().GetSeconds ();
  
  double qDelay;
  if (m_tsqClassic.empty ()) { qDelay = 0; }
  else { qDelay = now - m_tsqClassic.front (); }
  
  double p = m_a * (qDelay - m_qDelayRef.GetSeconds ()) + m_b * (qDelay - m_qDelayOld.GetSeconds ());

  if (m_dropProb < 0.000001) { p /= 2048; }
  else if (m_dropProb < 0.00001) { p /= 512; }
  else if (m_dropProb < 0.0001) { p /= 128; }
  else if (m_dropProb < 0.001) { p /= 32; }
  else if (m_dropProb < 0.01) { p /= 8; }
  else if (m_dropProb < 0.1) { p /= 2; }

  m_dropProb += p;

  if (qDelay == 0 && m_qDelayOld == 0) { m_dropProb *= 0.98; }

  if (m_dropProb < 0) { m_dropProb = 0; }
  if (m_dropProb > 1) { m_dropProb = 1; }
  
  m_qDelayOld = Time (Seconds (qDelay));

  m_burstAllowance = std::max(Seconds (0), m_burstAllowance - m_tUpdate);
  
  m_rtrsEvent = Simulator::Schedule (m_tUpdate, &L4SMyPieQueueDisc::CalculateP, this);
}

Ptr<QueueDiscItem>
L4SMyPieQueueDisc::DoDequeue ()
{
  NS_LOG_FUNCTION (this);
  
  double now = Simulator::Now ().GetSeconds ();
  
  if (GetInternalQueue (1)->IsEmpty () && GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }
  
  Ptr<QueueDiscItem> item;
  if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ())) != 0)
    {
	  m_qDelayFileL4S << now << "," << now - m_tsqL4S.front () << "," << GetInternalQueue (0)->GetNPackets () << "\n";
	  
	  m_throughputL4S += DynamicCast<Ipv4QueueDiscItem> (item)->GetPacketSize () + 2;
	  
	  m_tsqL4S.pop ();
	  
	  return item;
	}
  if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (1)->Dequeue ())) != 0)
    {
	  m_qDelayFileClassic << now << "," << now - m_tsqClassic.front () << "," << GetInternalQueue (1)->GetNPackets () << "\n";
	  
	  m_throughputClassic += DynamicCast<Ipv4QueueDiscItem> (item)->GetPacketSize () + 2;
	  
	  m_tsqClassic.pop ();
	  
	  return item;
	}

  return 0;
}

Ptr<const QueueDiscItem>
L4SMyPieQueueDisc::DoPeek () const
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
L4SMyPieQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("L4SMyPieQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("L4SMyPieQueueDisc cannot have packet filters");
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
      NS_LOG_ERROR ("L4SMyPieQueueDisc needs 2 internal queues");
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
