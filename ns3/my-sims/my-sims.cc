#include "ns3/dce-module.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/data-rate.h"

#include "ns3/applications-module.h"
#include "ns3/trace-helper.h"
#include <sys/stat.h>
#include <sys/types.h>

#include "ns3/traffic-control-module.h"

#include <iostream>
#include <fstream>

using namespace ns3;

// process throughput
void queuePrinter (QueueDiscContainer queueDiscs, std::ofstream* throughputFile, std::string aqm)
{
  double now = Simulator::Now ().GetSeconds ();
  
  std::cout << now << "s : " << queueDiscs.Get (0)->GetInternalQueue (0)->GetNPackets () << " ";
  std::cout << queueDiscs.Get (0)->GetInternalQueue (1)->GetNPackets () << "\n";
  
  if ((aqm.compare ("l4s-pi2") == 0))
    {
	  Ptr<L4SPi2QueueDisc> queueDisc = DynamicCast<L4SPi2QueueDisc> (queueDiscs.Get (0));
	  (*throughputFile) << now << ",";
	  (*throughputFile) << queueDisc->m_throughputL4S / 125000.0 << ",";
	  (*throughputFile) << queueDisc->m_throughputClassic / 125000.0 << "\n";
	  queueDisc->m_throughputL4S = 0;
	  queueDisc->m_throughputClassic = 0;
	}
  else if ((aqm.compare ("l4s-pie") == 0))
    {
	  Ptr<L4SMyPieQueueDisc> queueDisc = DynamicCast<L4SMyPieQueueDisc> (queueDiscs.Get (0));
	  (*throughputFile) << now << ",";
	  (*throughputFile) << queueDisc->m_throughputL4S / 125000.0 << ",";
	  (*throughputFile) << queueDisc->m_throughputClassic / 125000.0 << "\n";
	  queueDisc->m_throughputL4S = 0;
	  queueDisc->m_throughputClassic = 0;
	}
  else if ((aqm.compare ("l4s-cred") == 0))
    {
	  Ptr<L4SCREDQueueDisc> queueDisc = DynamicCast<L4SCREDQueueDisc> (queueDiscs.Get (0));
	  (*throughputFile) << now << ",";
	  (*throughputFile) << queueDisc->m_throughputL4S / 125000.0 << ",";
	  (*throughputFile) << queueDisc->m_throughputClassic / 125000.0 << "\n";
	  queueDisc->m_throughputL4S = 0;
	  queueDisc->m_throughputClassic = 0;
	}
  else if ((aqm.compare ("dqrr-pie") == 0) || (aqm.compare ("dqwrr-pie") == 0))
    {
	  Ptr<DQMyPieQueueDisc> queueDisc = DynamicCast<DQMyPieQueueDisc> (queueDiscs.Get (0));
	  (*throughputFile) << now << ",";
	  (*throughputFile) << queueDisc->m_throughputL4S / 125000.0 << ",";
	  (*throughputFile) << queueDisc->m_throughputClassic / 125000.0 << "\n";
	  queueDisc->m_throughputL4S = 0;
	  queueDisc->m_throughputClassic = 0;
	}
  else if ((aqm.compare ("dqrr-red") == 0) || (aqm.compare ("dqwrr-red") == 0))
    {
	  Ptr<DQREDQueueDisc> queueDisc = DynamicCast<DQREDQueueDisc> (queueDiscs.Get (0));
	  (*throughputFile) << now << ",";
	  (*throughputFile) << queueDisc->m_throughputL4S / 125000.0 << ",";
	  (*throughputFile) << queueDisc->m_throughputClassic / 125000.0 << "\n";
	  queueDisc->m_throughputL4S = 0;
	  queueDisc->m_throughputClassic = 0;
	}
  else if ((aqm.compare ("sq-pie") == 0))
    {
	  Ptr<DQMyPieQueueDisc> queueDisc = DynamicCast<DQMyPieQueueDisc> (queueDiscs.Get (0));
	  (*throughputFile) << now << ",";
	  (*throughputFile) << queueDisc->m_throughputL4S / 125000.0 << ",";
	  (*throughputFile) << queueDisc->m_throughputClassic / 125000.0 << "\n";
	  queueDisc->m_throughputL4S = 0;
	  queueDisc->m_throughputClassic = 0;
	}
  else if ((aqm.compare ("sq-red") == 0))
    {
	  Ptr<DQREDQueueDisc> queueDisc = DynamicCast<DQREDQueueDisc> (queueDiscs.Get (0));
	  (*throughputFile) << now << ",";
	  (*throughputFile) << queueDisc->m_throughputL4S / 125000.0 << ",";
	  (*throughputFile) << queueDisc->m_throughputClassic / 125000.0 << "\n";
	  queueDisc->m_throughputL4S = 0;
	  queueDisc->m_throughputClassic = 0;
	}
  Simulator::Schedule (Seconds (1), &queuePrinter, queueDiscs, throughputFile, aqm);
}

void statsPrinter (QueueDiscContainer queueDiscs)
{
  std::cout << "Unforced Drops : " << DynamicCast<L4SPi2QueueDisc> (queueDiscs.Get (0))->GetStats ().unforcedDrop << "\n";
  std::cout << "Forced Drops : " << DynamicCast<L4SPi2QueueDisc> (queueDiscs.Get (0))->GetStats ().forcedDrop << "\n";
  std::cout << "L4S Marks : " << DynamicCast<L4SPi2QueueDisc> (queueDiscs.Get (0))->GetStats ().l4sMark << "\n";
  std::cout << "Classic Marks : " << DynamicCast<L4SPi2QueueDisc> (queueDiscs.Get (0))->GetStats ().classicMark << "\n";
}

// move kernel dump with cwnd to main folder
void moveFiles ()
{
  rename ("files-0/var/log/messages", "messagesL");
  rename ("files-1/var/log/messages", "messagesC");
}


int main (int argc, char *argv[])
{
  // setting up simulation parameters
  uint32_t l4sStart = 0;
  uint32_t l4sStop = 90;
  uint32_t classicStart = 0;
  uint32_t classicStop = 90;
  
  uint32_t nLF = 1;
  uint32_t nCF = 1;
  
  std::string aqm = "l4s-pi2";
  std::string ccClassic = "reno";
  std::string ccL4S = "dctcp";
  
  double qDelayRef = 0.015;
  
  bool ecn = true;
  
  bool relentless = false;
  
  CommandLine cmd;
  
  cmd.AddValue ("l4sStart", "Starting Time of L4S Traffic", l4sStart);
  cmd.AddValue ("l4sStop", "Stopping Time of L4S Traffic", l4sStop);
  cmd.AddValue ("classicStart", "Starting Time of Classic Traffic", classicStart);
  cmd.AddValue ("classicStop", "Stopping Time of Classic Traffic", classicStop);
  cmd.AddValue ("nLF", "Number of L4S Flows", nLF);
  cmd.AddValue ("nCF", "Number of Classic Flows", nCF);
  cmd.AddValue ("aqm", "AQM Type", aqm);
  cmd.AddValue ("ecn", "ECN on Classic Link", ecn);
  cmd.AddValue ("ccClassic", "Congestion Control for Classic Sender", ccClassic);
  cmd.AddValue ("ccL4S", "Congestion Control for L4S Sender", ccL4S);
  cmd.AddValue ("qDelayRef", "Reference Queue Delay for L4S", qDelayRef);
  cmd.AddValue ("relentless", "Use Relentless", relentless);
  
  cmd.Parse (argc, argv);
  
  //Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));


  //Create Nodes
  Ptr<Node> senderL = CreateObject<Node> ();
  Ptr<Node> senderC = CreateObject<Node> ();
  Ptr<Node> middlebox = CreateObject<Node> ();
  Ptr<Node> receiver = CreateObject<Node> ();
  Names::Add ("senderL", senderL);
  Names::Add ("senderC", senderC);
  Names::Add ("middlebox", middlebox);
  Names::Add ("receiver", receiver);
  

  // Install Links
  PointToPointHelper p2p;
  
  p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  NetDeviceContainer devLM = p2p.Install (senderL, middlebox);
  NetDeviceContainer devCM = p2p.Install (senderC, middlebox);

  p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (1));
  p2p.SetChannelAttribute ("Delay", StringValue ("0ms"));
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  NetDeviceContainer devMR = p2p.Install (middlebox, receiver);
  
  
  //Install Modified LinuxStack on L4S Sender
  DceManagerHelper dceManager;
  LinuxStackHelper linuxStack;
  
  if (aqm.compare ("sq-pie") == 0 || aqm.compare ("sq-red") == 0) { std::cout << "Single Queue\n"; dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux-47-cwnd.so")); }
  else
	{
	  if (relentless) { std::cout << "Relentless\n"; dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux-47-cwnd-ect1-relentless.so")); }
	  else { dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux-47-cwnd-ect1.so")); }
	}
  
  dceManager.Install (senderL);
  linuxStack.SysctlSet (senderL, ".net.ipv4.tcp_ecn", "1");
  linuxStack.SysctlSet (senderL, ".net.ipv4.tcp_window_scaling", "1");
  linuxStack.SysctlSet (senderL, ".net.ipv4.tcp_sack", "1");
  linuxStack.SysctlSet (senderL, ".net.ipv4.tcp_congestion_control", ccL4S);
  linuxStack.Install (senderL);
  
  
  //Install LinuxStack on Classic Sender and Receiver
  dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux-47-cwnd.so"));

  dceManager.Install (senderC);
  if (ecn) { linuxStack.SysctlSet (senderC, ".net.ipv4.tcp_ecn", "1"); }
  else { linuxStack.SysctlSet (senderC, ".net.ipv4.tcp_ecn", "0"); }
  linuxStack.SysctlSet (senderC, ".net.ipv4.tcp_window_scaling", "1");
  linuxStack.SysctlSet (senderC, ".net.ipv4.tcp_sack", "1");
  linuxStack.SysctlSet (senderC, ".net.ipv4.tcp_congestion_control", ccClassic);
  linuxStack.Install (senderC);
  
  dceManager.Install (receiver);
  linuxStack.SysctlSet (receiver, ".net.ipv4.tcp_ecn", "2");
  linuxStack.SysctlSet (receiver, ".net.ipv4.tcp_window_scaling", "1");
  linuxStack.SysctlSet (receiver, ".net.ipv4.tcp_sack", "1");
  linuxStack.SysctlSet (receiver, ".net.ipv4.tcp_congestion_control", "dctcp");
  linuxStack.Install (receiver);

  
  //Install NS3 Stack on Middleboxes
  InternetStackHelper ns3Stack;
  ns3Stack.Install (middlebox); 
  
  
  // Set Up AQM on Middlebox
  TrafficControlHelper tchL4S;
  uint16_t handle = 0;
  if (aqm.compare ("l4s-pi2") == 0)
    {
	  std::cout << "L4S PI2\n";
	  handle = tchL4S.SetRootQueueDisc ("ns3::L4SPi2QueueDisc");
	}
  else if (aqm.compare ("l4s-pie") == 0)
    {
	  std::cout << "L4S PIE\n";
	  handle = tchL4S.SetRootQueueDisc ("ns3::L4SMyPieQueueDisc");
	}
  else if (aqm.compare ("l4s-cred") == 0)
    {
	  std::cout << "L4S CRED\n";
	  handle = tchL4S.SetRootQueueDisc ("ns3::L4SCREDQueueDisc");
	}
  else if (aqm.compare ("dqwrr-pie") == 0)
    {
	  std::cout << "DQ PIE WRR " << 1000*qDelayRef << "ms\n";
	  
	  handle = tchL4S.SetRootQueueDisc ("ns3::DQMyPieQueueDisc", "nLF", UintegerValue (nLF), "nCF", UintegerValue (nCF), "QueueDelayReferenceL4S", TimeValue (Seconds (qDelayRef)), "TupdateL4S", TimeValue (Seconds (qDelayRef)));
	}
  else if (aqm.compare ("dqrr-pie") == 0)
    {
	  std::cout << "DQ PIE RR " << 1000*qDelayRef << "ms\n";
	  
	  handle = tchL4S.SetRootQueueDisc ("ns3::DQMyPieQueueDisc", "nLF", UintegerValue (1), "nCF", UintegerValue (1), "QueueDelayReferenceL4S", TimeValue (Seconds (qDelayRef)), "TupdateL4S", TimeValue (Seconds (qDelayRef)));
	}
  else if (aqm.compare ("dqwrr-red") == 0)
    {
	  double share = 1;
	  if (nCF > 0) { share = double (nLF) / double (nCF + nLF); }
	  double max = 80;
	  if (ccL4S.compare ("dctcp") == 0)
	    {
		  if (qDelayRef == 0.001) { max = 1; }
		  else if (qDelayRef == 0.005) { max = 5; }
		  else if (qDelayRef == 0.015) { max = 16; }
	    }
	  else
	    {
		  if (qDelayRef == 0.001) { max = 7; }
		  else if (qDelayRef == 0.005) { max = 34; }
	    }
	  
	  max = max * share;
	  double min = round (max / 3.0);
	  max = std::max (round (max), 1.0);
	  min = std::min (min, max - 1.0);
	  
	  std::cout << "DQ RED WRR " << min << " " << max << "\n";
	  
	  handle = tchL4S.SetRootQueueDisc ("ns3::DQREDQueueDisc", "nLF", UintegerValue (nLF), "nCF", UintegerValue (nCF), "MinThL4S", UintegerValue (min), "MaxThL4S", UintegerValue (max));
	}
  else if (aqm.compare ("dqrr-red") == 0)
    {
	  double share = 1;
	  if (nCF > 0) { share = 0.5; }
	  double max = 80;
	  if (ccL4S.compare ("dctcp") == 0)
	    {
		  if (qDelayRef == 0.001) { max = 1; }
		  else if (qDelayRef == 0.005) { max = 5; }
		  else if (qDelayRef == 0.015) { max = 16; }
	    }
	  else
	    {
		  if (qDelayRef == 0.001) { max = 7; }
		  else if (qDelayRef == 0.005) { max = 34; }
	    }
	  
	  max = max * share;
	  double min = round (max / 3.0);
	  max = std::max (round (max), 1.0);
	  min = std::min (min, max - 1.0);
	  
	  std::cout << "DQ RED RR " << min << " " << max << "\n";
	  
	  handle = tchL4S.SetRootQueueDisc ("ns3::DQREDQueueDisc", "nLF", UintegerValue (1), "nCF", UintegerValue (1), "MinThL4S", UintegerValue (min), "MaxThL4S", UintegerValue (max));
	}
  else if (aqm.compare ("sq-pie") == 0)
    {
	  std::cout << "SQ PIE " << 1000*qDelayRef << "ms\n";
	  
	  handle = tchL4S.SetRootQueueDisc ("ns3::DQMyPieQueueDisc", "nLF", UintegerValue (nLF), "nCF", UintegerValue (nCF), "QueueDelayReferenceClassic", TimeValue (Seconds (qDelayRef)), "TupdateClassic", TimeValue (Seconds (qDelayRef)));
	}
  else if (aqm.compare ("sq-red") == 0)
    {
	  double max = 80;
	  if (qDelayRef == 0.001) { max = 7; }
	  else if (qDelayRef == 0.005) { max = 34; }
	  
	  double min = std::min (round (max / 3.0), max - 1.0);
	  
	  std::cout << "SQ RED " << min << " " << max << "\n";
	  
	  handle = tchL4S.SetRootQueueDisc ("ns3::DQREDQueueDisc", "nLF", UintegerValue (nLF), "nCF", UintegerValue (nCF), "MinThClassic", UintegerValue (min), "MaxThClassic", UintegerValue (max));
	}
  tchL4S.AddInternalQueues (handle, 2, "ns3::DropTailQueue", "MaxPackets", UintegerValue (10000));
  QueueDiscContainer queueDiscs = tchL4S.Install (devMR.Get (0));
    
  
  //Install Addresses
  Ipv4AddressHelper address;
  
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer intLM = address.Assign (devLM);
  
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer intCM = address.Assign (devCM);
  
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer intMR = address.Assign (devMR);

  
  //Install Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  LinuxStackHelper::PopulateRoutingTables ();


  //Output of Routing Tables
  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("dynamic-global-routing.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (10), routingStream);


  //Install Packet Sink
  PacketSinkHelper sink = PacketSinkHelper ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("10.1.3.2", 50000));
  
  ApplicationContainer cR;
  cR = sink.Install (receiver);
  cR.Start (Seconds (1));
  

  //Install Sending Applications
  OnOffHelper onoff ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("10.1.3.2", 50000));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1000));
  onoff.SetAttribute ("DataRate", StringValue ("1000Kbps"));
  
  BulkSendHelper bulk ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("10.1.3.2", 50000));
  bulk.SetAttribute ("MaxBytes", UintegerValue (0));
  
  //l4sStop = 60;
  //classicStop = l4sStop;
  
  std::cout << nLF << ":" << nCF << "\n";
  
  ApplicationContainer cL;
  for (int i = 0; i < nLF; i++)
  {
    cL.Add (bulk.Install (senderL));
  }
  cL.Start (Seconds (l4sStart + 2));
  cL.Stop (Seconds (l4sStop + 2));  
  
  ApplicationContainer cC;
  for (int i = 0; i < nCF; i++)
  {
    cC.Add (bulk.Install (senderC));
  }
  cC.Start (Seconds (classicStart + 2));
  cC.Stop (Seconds (classicStop + 2));
  
  
  std::ofstream* throughputFile = new std::ofstream ();
  throughputFile->open ("throughput");
  
  Simulator::Schedule (Seconds (1), &queuePrinter, queueDiscs, throughputFile, aqm);
  
  //Simulator::Schedule (Seconds (max (l4sStop, classicStop) + 2), &statsPrinter, queueDiscs);
  
  //Enable Pcap Traces
  p2p.EnablePcapAll ("my-dce-test", false);
  
  Simulator::Schedule (Seconds (std::max (l4sStop, classicStop) + 2), &moveFiles);
  
  Simulator::Stop (Seconds (std::max (l4sStop, classicStop) + 2));
  Simulator::Run ();

  Simulator::Destroy ();
  
  throughputFile->close ();
  
  if (aqm.compare ("sq-red") == 0 || aqm.compare ("sq-pie") == 0)
    {
	    std::ofstream* qDelayFileL4S = new std::ofstream ();
		qDelayFileL4S->open ("qDelayL4S");
		(*qDelayFileL4S) << "2,0,0\n32,0,0\n62,0,0\n92,0,0";
		qDelayFileL4S->close ();
	}

  return 0;
}
