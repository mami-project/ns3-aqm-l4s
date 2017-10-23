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
#include "l4s-cred-queue-disc.h"
#include "ns3/drop-tail-queue.h"

#include "ns3/ipv4-queue-disc-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("L4SCREDQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (L4SCREDQueueDisc);

TypeId L4SCREDQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::L4SCREDQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<L4SCREDQueueDisc> ()
    .AddAttribute ("A",
                   "Value of alpha",
                   DoubleValue (20),
                   MakeDoubleAccessor (&L4SCREDQueueDisc::m_a),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("B",
                   "Value of beta",
                   DoubleValue (200),
                   MakeDoubleAccessor (&L4SCREDQueueDisc::m_b),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Tupdate",
                   "Time period to calculate drop probability",
                   TimeValue (Seconds (0.032)),
                   MakeTimeAccessor (&L4SCREDQueueDisc::m_tUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("Supdate",
                   "Start time of the update timer",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&L4SCREDQueueDisc::m_sUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("QueueLimit",
                   "Queue limit in bytes/packets",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&L4SCREDQueueDisc::m_queueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("QueueDelayReference",
                   "Desired queue delay",
                   TimeValue (Seconds (0.015)),
                   MakeTimeAccessor (&L4SCREDQueueDisc::m_qDelayRef),
                   MakeTimeChecker ())
    .AddAttribute ("MaxBurstAllowance",
                   "Current max burst allowance in seconds before random drop",
                   TimeValue (Seconds (0.15)),
                   MakeTimeAccessor (&L4SCREDQueueDisc::m_maxBurst),
                   MakeTimeChecker ())
    .AddAttribute ("K",
                   "K for BW division",
                   DoubleValue (2),
                   MakeDoubleAccessor (&L4SCREDQueueDisc::m_k),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("DropScale",
                   "Scaling Factor for Classic Drops",
                   DoubleValue (-1),
                   MakeDoubleAccessor (&L4SCREDQueueDisc::m_dropScale),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SmoothingFactor",
                   "Smoothing Factor for Classic Drops",
                   DoubleValue (5),
                   MakeDoubleAccessor (&L4SCREDQueueDisc::m_smoothingFactor),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ByteThreshold",
                   "Byte Threshold for L4S Marks",
                   UintegerValue (7510), // 5 * MTU
                   MakeUintegerAccessor (&L4SCREDQueueDisc::m_byteThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("L4SThreshold",
                   "L4SThreshold",
                   // Pi2 intends max (1ms, serialization time of 2 MTU)
                   // default 2 ms <=> 6Mbps
                   TimeValue (Seconds (0.002)),
                   MakeTimeAccessor (&L4SCREDQueueDisc::m_l4sThreshold),
                   MakeTimeChecker ())
    .AddAttribute ("TimeShift",
                   "Time shift for L4S Classic Priorisation",
                   TimeValue (Seconds (0.04)),
                   MakeTimeAccessor (&L4SCREDQueueDisc::m_tShift),
                   MakeTimeChecker ())
  ;

  return tid;
}

L4SCREDQueueDisc::L4SCREDQueueDisc ()
  : QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();
  //m_rtrsEvent = Simulator::Schedule (m_sUpdate, &L4SCREDQueueDisc::CalculateP, this);
  
  m_qDelayFileL4S.open ("qDelayL4S");
  m_qDelayFileClassic.open ("qDelayClassic");
  m_actionsFileL4S.open ("actionsL4S");
  m_actionsFileClassic.open ("actionsClassic");
}

L4SCREDQueueDisc::~L4SCREDQueueDisc ()
{
  NS_LOG_FUNCTION (this);
  
  m_qDelayFileL4S.close ();
  m_qDelayFileClassic.close ();
  m_actionsFileL4S.close ();
  m_actionsFileClassic.close ();
}

void
L4SCREDQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  Simulator::Remove (m_rtrsEvent);
  QueueDisc::DoDispose ();
}

L4SCREDQueueDisc::Stats
L4SCREDQueueDisc::GetStats ()
{
  NS_LOG_FUNCTION (this);
  return m_stats;
}

bool
L4SCREDQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  double now = Simulator::Now ().GetSeconds ();
  
  // Distinguish Classic L4S
  int qDist = 1;
  ns3::Ipv4Header::EcnType ecn = DynamicCast<Ipv4QueueDiscItem> (item)->GetHeader ().GetEcn ();
  if (ecn == ns3::Ipv4Header::ECN_ECT1 || ecn == ns3::Ipv4Header::ECN_CE) { qDist = 0; }
  
  if ((GetInternalQueue (0)->GetNPackets () + GetInternalQueue (1)->GetNPackets ()) >= m_queueLimit)
    {
      std::cout << now << " FORCED DROP!\n";
      
      if (qDist == 0) { m_actionsFileL4S << now << ",F\n"; }
      else { m_actionsFileClassic << now << ",F\n"; }
      
      // Drops due to queue limit: reactive
      Drop (item);
      m_stats.forcedDrop++;	  
      return false;
    }
  
  // time stamp
  if (qDist == 0) { m_tsqL4S.push (now); }
  else { m_tsqClassic.push (now); }

  bool retval = GetInternalQueue (qDist)->Enqueue (item);

  NS_LOG_LOGIC ("\t packetsInQueue  " << GetInternalQueue (1)->GetNPackets ());

  return retval;
}

void
L4SCREDQueueDisc::InitializeParams (void)
{
  // Initially queue is empty so variables are initialize to zero except m_dqCount
  m_dropProb = 0;
  m_burstState = NO_BURST;
  m_qDelayOld = Time (Seconds (0));
  m_stats.forcedDrop = 0;
  m_stats.unforcedDrop = 0;
  
  m_stats.l4sMark = 0;
  m_stats.classicMark = 0;
  
  m_alphaU = m_tUpdate.GetSeconds () * m_a;
  m_betaU = m_tUpdate.GetSeconds () * m_b;
  
  m_wQDelayAvg = 0;
  
  m_dropScaleL4S = pow (2, m_dropScale) / m_k; // 2^(S_C + k') = 2^(S_C) * 2^(k') = 2^(S_C) * k
  m_dropScaleClassic = pow (2, m_dropScale);
  m_a = pow (2, -m_smoothingFactor);
  
  m_throughputClassic = 0;
  m_throughputL4S = 0;
}

void L4SCREDQueueDisc::CalculateP ()
{
  // update drop probability
  NS_LOG_FUNCTION (this);
  
  double now = Simulator::Now ().GetSeconds ();
  double qDelay = 0;
  
  if (!m_tsqClassic.empty ()) { qDelay = now - m_tsqClassic.front (); }
  else if (!m_tsqL4S.empty ()) { qDelay = now - m_tsqL4S.front (); }
  
  m_dropProb += m_alphaU * (qDelay - m_qDelayRef.GetSeconds ()) + m_betaU * (qDelay - m_qDelayOld.GetSeconds ());
  
  m_qDelayOld = Time (Seconds (qDelay));
  
  m_rtrsEvent = Simulator::Schedule (m_tUpdate, &L4SCREDQueueDisc::CalculateP, this);
}

Ptr<QueueDiscItem>
L4SCREDQueueDisc::DoDequeue ()
{
  NS_LOG_FUNCTION (this);
  
  while (true)
    {  
	  if (GetInternalQueue (1)->IsEmpty () && GetInternalQueue (0)->IsEmpty ())
		{
		  NS_LOG_LOGIC ("Queue empty");
		  return 0;
		}
	  
	  double now = Simulator::Now ().GetSeconds ();
	  
	  double qDelayClassic = 0;
	  double qDelayL4S = 0;
	  if (!m_tsqClassic.empty ()) { qDelayClassic = now - m_tsqClassic.front (); }
	  if (!m_tsqL4S.empty ()) { qDelayL4S = now - m_tsqL4S.front (); }
	  
	  double u1 = m_uv->GetValue ();
	  double u2 = m_uv->GetValue ();
		  
	  Ptr<QueueDiscItem> item;
	  if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ())) != 0)
		{
		  m_tsqL4S.pop ();
		  
		  double markProb = qDelayClassic / m_dropScaleL4S;
		  
		  // Mark if L4S Queue Length exceeds threshold or randomly
		  if ((GetInternalQueue (0)->GetNBytes () > m_byteThreshold) || (markProb > u1))
			{
			  if (DynamicCast<Ipv4QueueDiscItem> (item)->Mark () == false)
				{
				  Drop (item);
				  m_stats.unforcedDrop++;
				  
				  m_actionsFileL4S << now << ",D\n";
				  
				  continue;
				}
			  else
				{
				  m_stats.l4sMark++;
				  
				  m_actionsFileL4S << now << ",M\n";
				}
			}
		  m_throughputL4S += DynamicCast<Ipv4QueueDiscItem> (item)->GetPacketSize () + 2;
		  
		  m_qDelayFileL4S << now << "," << qDelayL4S << "," << GetInternalQueue (0)->GetNPackets () << "\n";
		  
		  return item;
		}
	  while ((item = StaticCast<QueueDiscItem> (GetInternalQueue (1)->Dequeue ())) != 0)
		{
		  m_tsqClassic.pop ();
		  
		  // Update drop probability
		  m_wQDelayAvg = m_a * qDelayClassic + (1 - m_a) * m_wQDelayAvg;
		  double dropProb = m_wQDelayAvg / m_dropScaleClassic;
		  
		  // Random Drop
		  if ( dropProb > std::max (u1, u2) )
			{
			  if (DynamicCast<Ipv4QueueDiscItem> (item)->Mark () == false)
				{
				  Drop (item);
				  m_stats.unforcedDrop++;
				  
				  m_actionsFileClassic << now << ",D\n";
				  
				  continue;
				}
			  else
				{
				  m_stats.classicMark++;
				  
				  m_actionsFileClassic << now << ",M\n";
				}
			}
		  m_throughputClassic += DynamicCast<Ipv4QueueDiscItem> (item)->GetPacketSize () + 2;
		
		  m_qDelayFileClassic << now << "," << qDelayClassic << "," << GetInternalQueue (1)->GetNPackets () << "\n";
		  
		  return item;
		}
	}
  return 0;
}

Ptr<const QueueDiscItem>
L4SCREDQueueDisc::DoPeek () const
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
L4SCREDQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("L4SCREDQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("L4SCREDQueueDisc cannot have packet filters");
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
      NS_LOG_ERROR ("L4SCREDQueueDisc needs 2 internal queues");
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
