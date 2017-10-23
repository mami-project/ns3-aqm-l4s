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
 * PORT NOTE: This code was ported from ns-2.36rc1 (queue/pie.h).
 * Most of the comments are also ported from the same.
 */

#ifndef L4S_MY_PIE_QUEUE_DISC_H
#define L4S_MY_PIE_QUEUE_DISC_H

#include <queue>
#include "ns3/packet.h"
#include "ns3/queue-disc.h"
#include "ns3/nstime.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/timer.h"
#include "ns3/event-id.h"
#include "ns3/random-variable-stream.h"

#include <iostream>
#include <fstream>

#define BURST_RESET_TIMEOUT 1.5

namespace ns3 {

class TraceContainer;
class UniformRandomVariable;

/**
 * \ingroup traffic-control
 *
 * \brief Implements PIE Active Queue Management discipline
 */
class L4SMyPieQueueDisc : public QueueDisc
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief L4SMyPieQueueDisc Constructor
   */
  L4SMyPieQueueDisc ();

  /**
   * \brief L4SMyPieQueueDisc Destructor
   */
  virtual ~L4SMyPieQueueDisc ();

  /**
   * \brief Stats
   */
  typedef struct
  {
    uint32_t unforcedDrop;      //!< Early probability drops: proactive
    uint32_t forcedDrop;        //!< Drops due to queue limit: reactive
    uint32_t l4sMark;
    uint32_t classicMark;
  } Stats;
  
  uint64_t m_throughputL4S;
  uint64_t m_throughputClassic;

  /**
   * \brief Burst types
   */
  enum BurstStateT
  {
    NO_BURST,
    IN_BURST,
    IN_BURST_PROTECTING,
  };

  /**
   * \brief Get PIE statistics after running.
   *
   * \returns The drop statistics.
   */
  Stats GetStats ();

protected:
  /**
   * \brief Dispose of the object
   */
  virtual void DoDispose (void);

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void) const;
  virtual bool CheckConfig (void);

  /**
   * \brief Initialize the queue parameters.
   */
  virtual void InitializeParams (void);

  /**
   * \brief Check if a packet needs to be dropped due to probability drop
   * \param item queue item
   * \param qSize queue size
   * \returns 0 for no drop, 1 for drop
   */
  bool DropEarly (Ptr<QueueDiscItem> item, bool ect1, double qDelayL4S);

  /**
   * Periodically update the drop probability based on the delay samples:
   * not only the current delay sample but also the trend where the delay
   * is going, up or down
   */
  void CalculateP ();

  Stats m_stats;                                //!< PIE statistics

  // ** Variables supplied by user
  Queue::QueueMode m_mode;                      //!< Mode (bytes or packets)
  uint32_t m_queueLimit;                        //!< Queue limit in bytes / packets
  Time m_sUpdate;                               //!< Start time of the update timer
  Time m_tUpdate;                               //!< Time period after which CalculateP () is called
  Time m_qDelayRef;                             //!< Desired queue delay
  uint32_t m_meanPktSize;                       //!< Average packet size in bytes
  Time m_maxBurst;                              //!< Maximum burst allowed before random early dropping kicks in
  double m_a;                                   //!< Parameter to pie controller
  double m_b;                                   //!< Parameter to pie controller
  uint32_t m_dqThreshold;                       //!< Minimum queue size in bytes before dequeue rate is measured

  // ** Variables maintained by PIE
  double m_dropProb;                            //!< Variable used in calculation of drop probability
  Time m_qDelayOld;                             //!< Old value of queue delay
  Time m_qDelay;                                //!< Current value of queue delay
  Time m_burstAllowance;                        //!< Current max burst value in seconds that is allowed before random drops kick in
  uint32_t m_burstReset;                        //!< Used to reset value of burst allowance
  BurstStateT m_burstState;                     //!< Used to determine the current state of burst
  bool m_inMeasurement;                         //!< Indicates whether we are in a measurement cycle
  double m_avgDqRate;                           //!< Time averaged dequeue rate
  double m_dqStart;                             //!< Start timestamp of current measurement cycle
  uint32_t m_dqCount;                           //!< Number of bytes departed since current measurement cycle starts
  EventId m_rtrsEvent;                          //!< Event used to decide the decision of interval of drop probability calculation
  Ptr<UniformRandomVariable> m_uv;              //!< Rng stream
  
  double m_lastDqL4S;
  double m_k;
  
  std::queue<double> m_tsqL4S;
  std::queue<double> m_tsqClassic;
  
  std::ofstream m_qDelayFileL4S;
  std::ofstream m_qDelayFileClassic;
  std::ofstream m_actionsFileL4S;
  std::ofstream m_actionsFileClassic;
  
  Time m_l4sThreshTime;
  uint32_t m_l4sThreshLen;
};

};   // namespace ns3

#endif

