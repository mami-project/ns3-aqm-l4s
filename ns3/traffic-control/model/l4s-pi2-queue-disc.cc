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
#include "l4s-pi2-queue-disc.h"
#include "ns3/drop-tail-queue.h"

#include "ns3/ipv4-queue-disc-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("L4SPi2QueueDisc");

NS_OBJECT_ENSURE_REGISTERED (L4SPi2QueueDisc);

TypeId L4SPi2QueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::L4SPi2QueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<L4SPi2QueueDisc> ()
    .AddAttribute ("A",
                   "Value of alpha",
                   DoubleValue (10),
                   MakeDoubleAccessor (&L4SPi2QueueDisc::m_a),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("B",
                   "Value of beta",
                   DoubleValue (100),
                   MakeDoubleAccessor (&L4SPi2QueueDisc::m_b),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Tupdate",
                   "Time period to calculate drop probability",
                   TimeValue (Seconds (0.016)),
                   MakeTimeAccessor (&L4SPi2QueueDisc::m_tUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("Supdate",
                   "Start time of the update timer",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&L4SPi2QueueDisc::m_sUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("QueueLimit",
                   "Queue limit in bytes/packets",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&L4SPi2QueueDisc::m_queueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("QueueDelayReference",
                   "Desired queue delay",
                   TimeValue (Seconds (0.015)),
                   MakeTimeAccessor (&L4SPi2QueueDisc::m_qDelayRef),
                   MakeTimeChecker ())
    .AddAttribute ("MaxBurstAllowance",
                   "Current max burst allowance in seconds before random drop",
                   TimeValue (Seconds (0.15)),
                   MakeTimeAccessor (&L4SPi2QueueDisc::m_maxBurst),
                   MakeTimeChecker ())
    .AddAttribute ("K",
                   "K for BW division",
                   DoubleValue (2),
                   MakeDoubleAccessor (&L4SPi2QueueDisc::m_k),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("L4SThreshLen",
                   "L4SThreshold in Length",
                   UintegerValue (3004), // 2 * MTU
                   MakeUintegerAccessor (&L4SPi2QueueDisc::m_l4sThreshLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("L4SThreshTime",
                   "L4SThreshold in Time",
                   TimeValue (Seconds (0.001)),
                   MakeTimeAccessor (&L4SPi2QueueDisc::m_l4sThreshTime),
                   MakeTimeChecker ())
    .AddAttribute ("DropProbClassicMax",
                   "Maximum Drop Probability for Classic before Drops",
                   DoubleValue (0.25),
                   MakeDoubleAccessor (&L4SPi2QueueDisc::m_dropProbClassicMax),
                   MakeDoubleChecker<double> ())
  ;

  return tid;
}

L4SPi2QueueDisc::L4SPi2QueueDisc ()
  : QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();
  m_rtrsEvent = Simulator::Schedule (m_sUpdate, &L4SPi2QueueDisc::CalculateP, this);
  
  m_qDelayFileL4S.open ("qDelayL4S");
  m_qDelayFileClassic.open ("qDelayClassic");
  m_actionsFileL4S.open ("actionsL4S");
  m_actionsFileClassic.open ("actionsClassic");
}

L4SPi2QueueDisc::~L4SPi2QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  
  m_qDelayFileL4S.close ();
  m_qDelayFileClassic.close ();
  m_actionsFileL4S.close ();
  m_actionsFileClassic.close ();
}

void
L4SPi2QueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  Simulator::Remove (m_rtrsEvent);
  QueueDisc::DoDispose ();
}

L4SPi2QueueDisc::Stats
L4SPi2QueueDisc::GetStats ()
{
  NS_LOG_FUNCTION (this);
  return m_stats;
}

bool
L4SPi2QueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
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
L4SPi2QueueDisc::InitializeParams (void)
{
  // Initially queue is empty so variables are initialize to zero except m_dqCount
  m_dropProbL4S = 0;
  m_dropProbClassic = 0;
  m_burstState = NO_BURST;
  m_qDelayOld = Time (Seconds (0));
  m_stats.forcedDrop = 0;
  m_stats.unforcedDrop = 0;
  
  m_stats.l4sMark = 0;
  m_stats.classicMark = 0;
  
  m_throughputClassic = 0;
  m_throughputL4S = 0;
  
  m_alphaU = m_tUpdate.GetSeconds () * m_a;
  m_betaU = m_tUpdate.GetSeconds () * m_b;
  
  m_tShift = Time (Seconds (2 * m_qDelayRef.GetSeconds ()));
  
  m_dropProbL4SMax = std::min (m_k * sqrt (m_dropProbClassicMax), 1.0);
}

void L4SPi2QueueDisc::CalculateP ()
{
  NS_LOG_FUNCTION (this);
  
  double now = Simulator::Now ().GetSeconds ();
  double qDelay = 0;
  
  if (!m_tsqClassic.empty ()) { qDelay = now - m_tsqClassic.front (); }
  else if (!m_tsqL4S.empty ()) { qDelay = now - m_tsqL4S.front (); }
  
  m_dropProbClassic += m_alphaU * (qDelay - m_qDelayRef.GetSeconds ()) + m_betaU * (qDelay - m_qDelayOld.GetSeconds ());
  
  m_dropProbL4S = m_dropProbClassic * m_k;
  
  m_qDelayOld = Time (Seconds (qDelay));
  
  m_rtrsEvent = Simulator::Schedule (m_tUpdate, &L4SPi2QueueDisc::CalculateP, this);
}

Ptr<QueueDiscItem>
L4SPi2QueueDisc::DoDequeue ()
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
	  double qDelayL4S = 0;
	  double qDelayClassic = 0;
	  if (!m_tsqL4S.empty ()) { qDelayL4S = now - m_tsqL4S.front (); }
	  if (!m_tsqClassic.empty ()) { qDelayClassic = now - m_tsqClassic.front (); }
	  
	  double u1 = m_uv->GetValue ();
	  double u2 = m_uv->GetValue ();
	  double u3 = m_uv->GetValue ();
	  
	  Ptr<QueueDiscItem> item;
	  // L4S priority if queue delay L4S < queue delay Classic - Time Shift
	  if ((qDelayL4S + m_tShift.GetSeconds ()) >= qDelayClassic)
		{
		  // dequeue L4S if possible
		  if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ())) != 0)
			{
			  m_tsqL4S.pop ();
			  
			  // staturation?
			  if (m_dropProbL4S < m_dropProbL4SMax)
			    {
				  // mark?
				  if (((qDelayL4S > m_l4sThreshTime.GetSeconds ()) && (GetInternalQueue (0)->GetNBytes () > m_l4sThreshLen)) || (m_dropProbL4S > u1))
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
				}
			  else
			    {
				  if (m_dropProbClassic > std::max (u1, u2))
				    {
					  Drop (item);
					  m_stats.unforcedDrop++;
					  
					  m_actionsFileL4S << now << ",O\n";
					  
					  continue;
					}
				  if (m_dropProbL4S > u3)
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
				}
			  
			  m_throughputL4S += DynamicCast<Ipv4QueueDiscItem> (item)->GetPacketSize () + 2;
			  
			  m_qDelayFileL4S << now << "," << qDelayL4S << "," << GetInternalQueue (1)->GetNPackets () << "\n";
			  
			  return item;
			}
		}
	  // dequeue Classic if possible
	  if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (1)->Dequeue ())) != 0)
		{
		  m_tsqClassic.pop ();
		  
		  // do drop?
		  if (m_dropProbClassic > std::max (u1, u2))
			{
			  // saturation?
			  if (m_dropProbL4S >= m_dropProbL4SMax)
			    {
				  Drop (item);
				  m_stats.unforcedDrop++;
				  
				  m_actionsFileClassic << now << ",O\n";
				  
				  continue;
				}
			  
			  // try to mark, if unsuccessful -> drop
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
L4SPi2QueueDisc::DoPeek () const
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
L4SPi2QueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("L4SPi2QueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("L4SPi2QueueDisc cannot have packet filters");
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
      NS_LOG_ERROR ("L4SPi2QueueDisc needs 2 internal queues");
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
