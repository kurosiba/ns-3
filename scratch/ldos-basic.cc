/*

toporogy 

(receiver)1----4----3----0(sender)
                    |
                    2(attacker)
*/



#include "ns3/nstime.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <cstring>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ldos-basic");

uint32_t oldTotalBytes = 0;
uint32_t newTotalBytes;

void TraceThroughput(Ptr<Application> app, Ptr<OutputStreamWrapper> stream)
{
  Ptr<PacketSink> pktSink = DynamicCast<PacketSink>(app);
  newTotalBytes = pktSink->GetTotalRx();
  // messure throughput in Kbps
  //fprintf(stdout,"%10.4f %f\n",Simulator::Now ().GetSeconds (), (newTotalBytes - oldTotalBytes)*8/0.1/1024);
  *stream->GetStream() << Simulator::Now().GetSeconds() << " \t " << (newTotalBytes - oldTotalBytes) * 8.0 / 0.1 / 1024 << std::endl;
  oldTotalBytes = newTotalBytes;
  Simulator::Schedule(Seconds(0.1), &TraceThroughput, app, stream);
}

int main(int argc, char *argv[])
{
  LogComponentEnable("ldos-basic", LOG_LEVEL_ALL);
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

  NodeContainer sources, sinks, attackers, routers;
  sources.Create(1);
  sinks.Create(1);
  attackers.Create(1);
  routers.Create(2);

  PointToPointHelper LinkBottoleNeck;
  LinkBottoleNeck.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  LinkBottoleNeck.SetChannelAttribute("Delay", StringValue("20ms"));
  //LinkBottoleNeck.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("20p"));

  PointToPointHelper Link100Mbps20ms;
  Link100Mbps20ms.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  Link100Mbps20ms.SetChannelAttribute("Delay", StringValue("20ms"));
  //Link100Mbps20ms.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("20p"));

  InternetStackHelper stack;
  stack.InstallAll();//TCP,UDP,IPv4,IPv6のノードをスタックしてる

  Ipv4AddressHelper a03, a23, a34, a41;
  a23.SetBase("10.0.1.0", "255.255.255.0");
  a03.SetBase("10.0.2.0", "255.255.255.0");
  a34.SetBase("10.0.3.0", "255.255.255.0");
  a41.SetBase("10.0.4.0", "255.255.255.0");

  NetDeviceContainer devices;
 
  /* attacker 同士の接続 */
  /*10.0.1.0*/
  devices = Link100Mbps20ms.Install(attackers.Get(0), routers.Get(0));
  a23.Assign(devices);
 
  /* sourcesの接続 */
  /*10.0.2.0*/ 
  devices = Link100Mbps20ms.Install(sources.Get(0), routers.Get(0));
  a03.Assign(devices);
  

  /* routerの接続 */
  /*10.0.3.0*/
  devices = LinkBottoleNeck.Install(routers.Get(1), routers.Get(0));
  a34.Assign(devices);
  
  /* sinksの接続 */
  /*10.0.4.0*/
  devices = Link100Mbps20ms.Install(routers.Get(1), sinks.Get(0));
  auto interface = a41.Assign(devices);

  const int tcp_sink_port = 3000;
  const uint128_t bulk_send_max_bytes = 1<<30;//1.0Gくらい
  const double max_simu_time = 60.0;

  BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(interface.GetAddress(1), tcp_sink_port));
  bulkSend.SetAttribute("MaxBytes", UintegerValue(bulk_send_max_bytes));
  ApplicationContainer bulkSendApp = bulkSend.Install(sources.Get(0));
  bulkSendApp.Start(Seconds(0.0));
  bulkSendApp.Stop(Seconds(max_simu_time));

  // TCP sink on the receiver (bob).
  PacketSinkHelper TCPsink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcp_sink_port));
  ApplicationContainer TCPSinkApp = TCPsink.Install(sinks.Get(0));
  TCPSinkApp.Start(Seconds(0.0));
  TCPSinkApp.Stop(Seconds(max_simu_time));


/*
ここからShrewのコード
*/

  // UDP On-Off Application - Application used by attacker (eve) to create the low-rate bursts.
  bool shrew = true;
  const int udp_sink_port = 9000;
  const std::string attacker_rate = "20Mbps";
  const double attacker_start = 0.1;
  const float burst_period = 1.2;
  const std::string on_time = "0.2";
  const std::string off_time = std::to_string(burst_period - stof(on_time));

  if (shrew)
  {
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interface.GetAddress(1), udp_sink_port)));
    onoff.SetConstantRate(DataRate(attacker_rate));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + on_time + "]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + off_time + "]"));

    ApplicationContainer onOffApp;
    
    onOffApp.Add(onoff.Install(attackers.Get(0)));
    
    onOffApp.Start(Seconds(attacker_start));
    onOffApp.Stop(Seconds(max_simu_time));

    // UDP sink on the receiver (bob).
    PacketSinkHelper UDPsink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), udp_sink_port)));
    ApplicationContainer UDPSinkApp = UDPsink.Install(sinks.Get(0));
    UDPSinkApp.Start(Seconds(0.0));
    UDPSinkApp.Stop(Seconds(max_simu_time));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  AsciiTraceHelper ascii;

/*
ここからcsvファイルとpcapファイルに出力する部分
*/

  // make trace file's name
  std::string fname = "data/ldos-basic/tcp.throughput.csv";
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(fname);
  Simulator::Schedule(Seconds(0.1), &TraceThroughput, sinks.Get(0)->GetApplication(0), stream);

  LinkBottoleNeck.EnablePcapAll("data/ldos-basic/pcaps");

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop(Seconds(max_simu_time));
  Simulator::Run();

  NS_LOG_UNCOND("Simulation success 🎉");
  NS_LOG_UNCOND("------ Simulation Results ------");
  NS_LOG_UNCOND("Total TCP Transfer: " << std::to_string(newTotalBytes) << "Bytes");
  double th = (double)newTotalBytes * 8 / (max_simu_time - 1) / 1024 / 1024;
  NS_LOG_UNCOND("Throughput        : " << std::to_string(th) << "Mbps");

  Simulator::Destroy();
  return 0;
}
