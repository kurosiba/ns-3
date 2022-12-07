#include "ns3/nstime.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/quic-module.h"
#include <cstring>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("quic-ldos-basic");

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
  LogComponentEnable("quic-ldos-basic", LOG_LEVEL_ALL);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  Config::SetDefault("ns3::QuicL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

  NodeContainer sources, sinks, attackers, routers;
  sources.Create(1);
  sinks.Create(1);
  attackers.Create(1);
  routers.Create(2);

  PointToPointHelper LinkBottoleNeck;
  LinkBottoleNeck.SetDeviceAttribute("DataRate", StringValue("15Mbps"));
  LinkBottoleNeck.SetChannelAttribute("Delay", StringValue("20ms"));

  PointToPointHelper Link100Mbps20ms;
  Link100Mbps20ms.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  Link100Mbps20ms.SetChannelAttribute("Delay", StringValue("20ms"));

  /*
  TCPのHelper
  */
  /*
  InternetStackHelper stack;
  stack.InstallAll();//TCP,UDP,IPv4,IPv6のノードをスタックしてる
  */
  
  /*
  QUICのHelper
  */
  QuicHelper stack;
  stack.InstallQuic(sources);
  stack.InstallQuic(sinks);
  stack.InstallQuic(attackers);
  stack.InstallQuic(routers);

  Ipv4AddressHelper a03, a23, a34, a41;
  a23.SetBase("10.0.1.0", "255.255.255.0");//("IP","サブネットマスク")
  a03.SetBase("10.0.2.0", "255.255.255.0");
  a34.SetBase("10.0.3.0", "255.255.255.0");
  a41.SetBase("10.0.4.0", "255.255.255.0");

  NetDeviceContainer devices;
  /* routerの接続 */
  devices = LinkBottoleNeck.Install(routers.Get(0), routers.Get(1));
  //address.NewNetwork();
  a34.Assign(devices);

  /* sourcesの接続 */
  
  devices = Link100Mbps20ms.Install(sources.Get(0), routers.Get(0));
  //address.NewNetwork();
  a03.Assign(devices);
  

  /* attackersの接続 */
  
  devices = Link100Mbps20ms.Install(attackers.Get(0), routers.Get(0));
  //address.NewNetwork();
  a23.Assign(devices);
  

  /* sinksの接続 */
  devices = Link100Mbps20ms.Install(routers.Get(1), sinks.Get(0));
  //address.NewNetwork();
  auto interface = a41.Assign(devices);

  /*
  sourceとsinksの送信設定
  */
 
  const int quic_sink_port = 443;//49153
  const uint128_t bulk_send_max_bytes = 1 << 30;//2Mbyte ,1<<30 1Gbyteくらい
  const double max_simu_time = 30.0;

  //これが送信するパケットがDISになっている
  BulkSendHelper bulkSend("ns3::QuicSocketFactory", InetSocketAddress(interface.GetAddress(1), quic_sink_port));//bulksend=一括送信
  bulkSend.SetAttribute("MaxBytes", UintegerValue(bulk_send_max_bytes));
  ApplicationContainer bulkSendApp = bulkSend.Install(sources.Get(0));
  bulkSendApp.Start(Seconds(0.0));
  bulkSendApp.Stop(Seconds(max_simu_time));

  //  sink on the receiver (bob).
  PacketSinkHelper QUICsink("ns3::QuicSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), quic_sink_port));//これをquicに変える必要がある
  ApplicationContainer QUICSinkApp = QUICsink.Install(sinks.Get(0));
  QUICSinkApp.Start(Seconds(0.0));
  QUICSinkApp.Stop(Seconds(max_simu_time));
  


  // UDP On-Off Application - Application used by attacker (eve) to create the low-rate bursts.
  bool shrew = true;
  const int udp_sink_port = 9000;
  const std::string attacker_rate = "30Mbps";
  const double attacker_start = 0.1;
  const float burst_period = 1.2;
  const std::string on_time = "0.2";
  const std::string off_time = std::to_string(burst_period - stof(on_time));


/*
ここからShrewのコード
*/
  if (shrew)
  {
    /*
    * データの送信、停止を繰り返すヘルパー
    */
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interface.GetAddress(1), udp_sink_port)));
    onoff.SetConstantRate(DataRate(attacker_rate));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + on_time + "]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + off_time + "]"));

    ApplicationContainer onOffApp;
    for (uint32_t i = 0; i < attackers.GetN(); i++)
    {
      onOffApp.Add(onoff.Install(attackers.Get(i)));
    }
    onOffApp.Start(Seconds(attacker_start));
    onOffApp.Stop(Seconds(max_simu_time));

    // UDP sink on the receiver (bob).
    PacketSinkHelper UDPsink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), udp_sink_port)));//これもUDPになってるけどどうなんだろう...
    ApplicationContainer UDPSinkApp = UDPsink.Install(sinks.Get(0));
    UDPSinkApp.Start(Seconds(0.0));
    UDPSinkApp.Stop(Seconds(max_simu_time));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  AsciiTraceHelper ascii;

/*
ここからcsvファイルに出力する部分
*/

  // make trace file's name
  std::string fname = "data/quic-ldos-basic/tcp.throughput.csv";
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(fname);
  Simulator::Schedule(Seconds(0.1), &TraceThroughput, sinks.Get(0)->GetApplication(0), stream);

  LinkBottoleNeck.EnablePcapAll("data/quic-ldos-basic/pcaps");

  /*
  animationをとるところ
  */
/*  
  std::string animFile = "data/ldos-basic-animation.xml";
  AnimationInterface anim (animFile);

  anim.SetConstantPosition(sources.Get(0), 1.0, 1.0);
  anim.UpdateNodeDescription(0, "node-1");
  anim.UpdateNodeSize(0, 0.5, 0.5);
    
  anim.SetConstantPosition(sinks.Get(0), 5.0, 5.0);
  anim.UpdateNodeDescription(1, "node-2");
  anim.UpdateNodeSize(1, 0.5, 0.5);

  anim.SetConstantPosition(attackers.Get(0), 10.0, 10.0);
  anim.UpdateNodeDescription(2, "node-3");
  anim.UpdateNodeSize(2, 0.5, 0.5);

  anim.SetConstantPosition(routers.Get(0), 10.0, 10.0);
  anim.UpdateNodeDescription(3, "node-4");
  anim.UpdateNodeSize(3, 0.5, 0.5);

  anim.SetConstantPosition(routers.Get(1), 10.0, 10.0);
  anim.UpdateNodeDescription(4, "node-5");
  anim.UpdateNodeSize(4, 0.5, 0.5);
*/  

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop(Seconds(max_simu_time));
  Simulator::Run();

  NS_LOG_UNCOND("Simulation success 🎉");
  NS_LOG_UNCOND("------ Simulation Results ------");
  NS_LOG_UNCOND("Total TCP Transfer: " << std::to_string(newTotalBytes) << "Bytes");
  double th = (double)newTotalBytes * 8 / (max_simu_time - 1) / 1024 / 1024;
  NS_LOG_UNCOND("Throughput        : " << std::to_string(th) << "Mbps");//これがflowmonitorというやつなのかもしれない

  Simulator::Destroy();
  return 0;
}
