/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

//
// Handoff scenario with Multipath TCPed iperf
//
// Simulation Topology:
// Scenario: H1 has 3G
//           during movement, MN keeps iperf session to SV.
//
//   <--------------------            ----------------------->
//                  LTE               Ethernet
//                   sim0 +----------+ sim1
//                  +------|  LTE  R  |------+
//                  |     +----------+      |
//              +---+                       +-----+
//          sim0|                                 |sim0
//     +----+---+                                 +----+---+
//     |   H1   |                                 |   H2   |
//     +---+----+                                 +----+---+
//          sim1|                                 |sim1
//              +--+                        +-----+
//                 | sim0 +----------+ sim1 |
//                  +-----|  WiFi R  |------+
//                        +----------+      
//                  WiFi              Ethernet
//   <--------------------            ----------------------->

#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/config-store-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("My-LTE-Test");

void progress ()
{
  std::cout << Simulator::Now ().GetSeconds () << " s -> Progress\n";
  Simulator::Schedule (Seconds (10), &progress);
}

void moveFiles ()
{
  rename ("files-1/var/log/messages", "messagesL");
  rename ("files-2/var/log/messages", "messagesC");
  rename ("my-lte-test-enb-2.pcap", "enb.pcap");
  rename ("my-lte-test-remote1-0.pcap", "senderL4S.pcap");
  rename ("my-lte-test-remote2-0.pcap", "senderClassic.pcap");
}

int main (int argc, char *argv[])
{
  // set up simulation parameters
  
  CommandLine cmd;
  
  uint32_t nCF = 1;
  uint32_t nLF = 1;
  
  uint32_t l4sStart = 0;
  uint32_t l4sStop = 90;
  uint32_t classicStart = 0;
  uint32_t classicStop = 90;
  
  bool ecn = true;
  std::string ccClassic = "reno";
  std::string ccL4S = "dctcp";
  bool relentless = false;
  bool sq = false;
  
  cmd.AddValue ("nCF", "n Classic Streams", nCF);
  cmd.AddValue ("nLF", "n L4S Streams", nLF);
  cmd.AddValue ("l4sStart", "Starting Time of L4S Traffic", l4sStart);
  cmd.AddValue ("l4sStop", "Stopping Time of L4S Traffic", l4sStop);
  cmd.AddValue ("classicStart", "Starting Time of Classic Traffic", classicStart);
  cmd.AddValue ("classicStop", "Stopping Time of Classic Traffic", classicStop);
  cmd.AddValue ("ccClassic", "Congestion Control for Classic Sender", ccClassic);
  cmd.AddValue ("ccL4S", "Congestion Control for L4S Sender", ccL4S);
  cmd.AddValue ("ecn", "Activate ECN on Classic Link", ecn);
  cmd.AddValue ("sq", "Single Queue Simulation", sq);
  cmd.AddValue ("relentless", "Use Relentless", relentless);
  
  cmd.Parse (argc, argv);
  
  //std::cout << nLF << " L4S Streams - " << nCF << " Classic Streams\n";

  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
  
  // LTE Helpers
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  lteHelper->SetSchedulerType ("ns3::PfFfMacScheduler");
  
  // Nodes
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  Ptr<Node> remote1 = CreateObject<Node> ();
  Ptr<Node> remote2 = CreateObject<Node> ();
  Ptr<Node> ue = CreateObject<Node> ();
  Ptr<Node> enb = CreateObject<Node> ();
  Names::Add ("pgw", pgw);
  Names::Add ("remote1", remote1);
  Names::Add ("remote2", remote2);
  Names::Add ("ue", ue);
  Names::Add ("enb", enb);
  
  // Linux Stack
  DceManagerHelper dceManager;
  LinuxStackHelper linuxStack;
  
  if (relentless) { std::cout << "Relentless\n"; dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux-47-cwnd-ect1-relentless.so")); }
  else { dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux-47-cwnd-ect1.so")); }
  
  dceManager.Install (remote1);
  linuxStack.SysctlSet (remote1, ".net.ipv4.tcp_ecn", "1");
  linuxStack.SysctlSet (remote1, ".net.ipv4.tcp_window_scaling", "1");
  linuxStack.SysctlSet (remote1, ".net.ipv4.tcp_sack", "1");
  linuxStack.SysctlSet (remote1, ".net.ipv4.tcp_congestion_control", ccL4S);
  linuxStack.Install (remote1);
  
  
  dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux-47-cwnd.so"));
  
  dceManager.Install (remote2);
  if (ecn) { linuxStack.SysctlSet (remote2, ".net.ipv4.tcp_ecn", "1"); }
  else { linuxStack.SysctlSet (remote2, ".net.ipv4.tcp_ecn", "0"); }
  linuxStack.SysctlSet (remote2, ".net.ipv4.tcp_window_scaling", "1");
  linuxStack.SysctlSet (remote2, ".net.ipv4.tcp_sack", "1");
  linuxStack.SysctlSet (remote2, ".net.ipv4.tcp_congestion_control", ccClassic);
  linuxStack.Install (remote2);

  dceManager.Install (ue);
  linuxStack.SysctlSet (ue, ".net.ipv4.tcp_ecn", "2");
  linuxStack.SysctlSet (ue, ".net.ipv4.tcp_window_scaling", "1");
  linuxStack.SysctlSet (ue, ".net.ipv4.tcp_sack", "1");
  linuxStack.SysctlSet (ue, ".net.ipv4.tcp_congestion_control", "dctcp");
  linuxStack.Install (ue);
  
  // Links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));
  NetDeviceContainer devPR1 = p2p.Install (remote1, pgw);
  NetDeviceContainer devPR2 = p2p.Install (remote2, pgw);

  Ipv4AddressHelper address;
  address.SetBase ("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer intPR1 = address.Assign (devPR1);
  address.SetBase ("2.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer intPR2 = address.Assign (devPR2);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector(0, 0, 0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator(positionAlloc);
  enbMobility.Install (enb);
  enbMobility.Install (ue);
  //MobilityHelper ueMobility;
  //ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle (-1000, 1000, -1000, 1000)), "Distance", DoubleValue (20));
  //ueMobility.Install(ue);
  //Ptr<MobilityModel> mob = ue->GetObject<MobilityModel> (); 
  
  // Attach UE to eNB
  NetDeviceContainer devEnb = lteHelper->InstallEnbDevice (enb);
  NetDeviceContainer devUe = lteHelper->InstallUeDevice (ue);
  Ipv4InterfaceContainer intUe = epcHelper->AssignUeIpv4Address (devUe);
  lteHelper->Attach (devUe.Get(0), devEnb.Get(0));
  
  // Activate Bearers
  // L4S Traffic specified by destination port 55555
  // Classic Traffic specified by dest port 50000
  // Classic Bearer need to be set up first
  Ptr<EpcTft> tft = Create<EpcTft> ();
  EpcTft::PacketFilter filter;
  filter.localPortStart = 50000;
  filter.localPortEnd = 50000;
  //filter.direction = EpcTft::DOWNLINK;
  tft->Add (filter);
  EpsBearer bearer (EpsBearer::NGBR_VIDEO_TCP_PREMIUM);
  lteHelper->ActivateDedicatedEpsBearer (devUe, bearer, tft);
  
  Ptr<EpcTft> tft2 = Create<EpcTft> ();
  EpcTft::PacketFilter filter2;
  filter2.localPortStart = 55555;
  filter2.localPortEnd = 55555;
  //filter2.direction = EpcTft::DOWNLINK;
  tft2->Add (filter2);
  EpsBearer bearer2 (EpsBearer::NGBR_VIDEO_TCP_PREMIUM);
  lteHelper->ActivateDedicatedEpsBearer (devUe, bearer2, tft2);
  
  // setup ip routes
  std::ostringstream cmd_oss;
  cmd_oss.str ("");
  cmd_oss << "rule add from " << intUe.GetAddress (0, 0) << " table " << 1;
  LinuxStackHelper::RunIp (ue, Seconds (0.1), cmd_oss.str ().c_str ());
  cmd_oss.str ("");
  cmd_oss << "route add default via " << "7.0.0.1 "  << " dev sim" << 0 << " table " << 1;
  LinuxStackHelper::RunIp (ue, Seconds (0.1), cmd_oss.str ().c_str ());

  cmd_oss.str ("");
  cmd_oss << "rule add from " << intPR1.GetAddress (0, 0) << " table " << (1);
  LinuxStackHelper::RunIp (remote1, Seconds (0.1), cmd_oss.str ().c_str ());
  cmd_oss.str ("");
  cmd_oss << "route add 1.0." << 0 << ".0/24 dev sim" << 0 << " scope link table " << (1);
  LinuxStackHelper::RunIp (remote1, Seconds (0.1), cmd_oss.str ().c_str ());
  
  cmd_oss.str ("");
  cmd_oss << "rule add from " << intPR2.GetAddress (0, 0) << " table " << (1);
  LinuxStackHelper::RunIp (remote2, Seconds (0.1), cmd_oss.str ().c_str ());
  cmd_oss.str ("");
  cmd_oss << "route add 2.0." << 0 << ".0/24 dev sim" << 0 << " scope link table " << (1);
  LinuxStackHelper::RunIp (remote2, Seconds (0.1), cmd_oss.str ().c_str ());

  

  // default route
  LinuxStackHelper::RunIp (ue, Seconds (0.1), "route add default via 7.0.0.1 dev sim0");
  LinuxStackHelper::RunIp (remote1, Seconds (0.1), "route add default via 1.0.0.2 dev sim0");
  LinuxStackHelper::RunIp (remote2, Seconds (0.1), "route add default via 2.0.0.2 dev sim0");
  LinuxStackHelper::RunIp (ue, Seconds (0.1), "rule show");
  LinuxStackHelper::RunIp (ue, Seconds (5.1), "route show table all");
  LinuxStackHelper::RunIp (remote1, Seconds (0.1), "rule show");
  LinuxStackHelper::RunIp (remote2, Seconds (5.1), "route show table all");

  // Sinks
  PacketSinkHelper sinkL4S ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 55555));
  ApplicationContainer sinkApp = sinkL4S.Install (ue);
  PacketSinkHelper sinkClassic ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 50000));
  sinkApp.Add (sinkClassic.Install (ue));
  PacketSinkHelper sinkDefault ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 44444));
  sinkApp.Add (sinkDefault.Install (ue));
  sinkApp.Start (Seconds (1));
  
  // OnOff
  OnOffHelper onoffL4S ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 55555));
  onoffL4S.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoffL4S.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoffL4S.SetAttribute ("PacketSize", UintegerValue (1000));
  onoffL4S.SetAttribute ("DataRate", StringValue ("1000Kbps"));
  
  OnOffHelper onoffClassic ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 50000));
  onoffClassic.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoffClassic.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoffClassic.SetAttribute ("PacketSize", UintegerValue (1000));
  onoffClassic.SetAttribute ("DataRate", StringValue ("1000Kbps"));
  
  OnOffHelper onoffBlindL4S ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 55555));
  onoffBlindL4S.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoffBlindL4S.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoffBlindL4S.SetAttribute ("PacketSize", UintegerValue (1000));
  onoffBlindL4S.SetAttribute ("DataRate", StringValue ("0.001Kbps"));
  
  OnOffHelper onoffBlindClassic ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 50000));
  onoffBlindClassic.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoffBlindClassic.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoffBlindClassic.SetAttribute ("PacketSize", UintegerValue (1000));
  onoffBlindClassic.SetAttribute ("DataRate", StringValue ("0.001Kbps"));
  
  // Bulk
  BulkSendHelper bulkL4S ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 55555));
  bulkL4S.SetAttribute ("MaxBytes", UintegerValue (0));
  
  BulkSendHelper bulkClassic ("ns3::LinuxTcpSocketFactory", InetSocketAddress ("7.0.0.2", 50000));
  bulkClassic.SetAttribute ("MaxBytes", UintegerValue (0));
  
  ApplicationContainer senderC, senderL;
  senderL.Add (onoffBlindL4S.Install (remote1));
  senderC.Add (onoffBlindClassic.Install (remote2));
  for (int i = 0; i < nLF; i++)
    {
	  senderL.Add (bulkL4S.Install (remote1));
	}
  for (int i = 0; i < nCF; i++)
    {
	  if (sq) { senderC.Add (bulkL4S.Install (remote2)); }
	  else { senderC.Add (bulkClassic.Install (remote2)); }
	}
  
  senderL.Start (Seconds (l4sStart + 2));
  senderL.Stop (Seconds (l4sStop + 2));
  senderC.Start (Seconds (classicStart + 2));
  senderC.Stop (Seconds (classicStop + 2));

  ApplicationContainer x;
  for (int i = 0; i < 10; i++)
    {
	  x.Add (onoffClassic.Install (remote2));
	}
  x.Start (Seconds (3600));

  //p2p.EnablePcapAll ("my-lte-test");
  //lteHelper->EnableTraces ();
  
  //Simulator::Schedule (Seconds (10), &progress);
  Simulator::Schedule (Seconds (std::max (l4sStop, classicStop) + 2), &moveFiles);
    
  Simulator::Stop (Seconds (std::max (l4sStop, classicStop) + 2));
  Simulator::Run ();
  Simulator::Destroy ();
  
  if (sq)
  {
	std::ofstream* qDelayFileClassic = new std::ofstream ();
	qDelayFileClassic->open ("qDelayClassic");
	(*qDelayFileClassic) << "2,0,0\n32,0,0\n62,0,0\n92,0,0";
	qDelayFileClassic->close ();
  }
  
  return 0;
}
