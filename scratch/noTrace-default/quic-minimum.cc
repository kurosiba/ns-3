
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

NS_LOG_COMPONENT_DEFINE("minumum");

uint32_t oldTotalBytes = 0;
uint32_t newTotalBytes;

void TraceThroughput(Ptr<Application> app, Ptr<OutputStreamWrapper> stream)
{
  Ptr<PacketSink> pktSink = DynamicCast<PacketSink>(app);
  newTotalBytes = pktSink->GetTotalRx();
  // messure throughput in Kbps
  //fprintf(stdout,"%10.4f %f\n",Simulator::Now ().GetSeconds (), (newTotalBytes - oldTotalBytes)*8/0.1/1024);
  *stream->GetStream() << Simulator::Now().GetSeconds() << " \t " << (newTotalBytes - oldTotalBytes) * 8.0 / 1.0 / 1000 << std::endl;
  oldTotalBytes = newTotalBytes;
  Simulator::Schedule(Seconds(1.0), &TraceThroughput, app, stream);
}

int main(int argc, char *argv[])
{
  LogComponentEnable("minumum", LOG_LEVEL_ALL);
  
  // LogComponentEnableAll(LOG_PREFIX_FUNC);
  // LogComponentEnableAll(LOG_PREFIX_TIME);
  // LogComponentEnableAll(LOG_PREFIX_NODE);
  // LogComponentEnableAll(LOG_PREFIX_LEVEL);
  // LogComponentEnableAll(LOG_PREFIX_ALL);

  // LogComponentEnable("IpL4Protocol", LOG_LEVEL_ALL);
  // LogComponentEnable("Socket", LOG_LEVEL_ALL);
  
  // LogComponentEnable("QuicClient", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicEchoClientApplication", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicEchoServerApplication", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicServer", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicHelper", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicCongestionControl", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicL4Protocol", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicL5Protocol", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicSocketBase", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicSocketFactory", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicSocketRxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicSocketTxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicSocket", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicStreamBase", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicStreamRxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicStreamTxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicStream", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicSubheader", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicTransportParameters", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicHeader", LOG_LEVEL_ALL);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
  //Config::SetDefault ("ns3::QueueBase::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  //Config::SetDefault ("ns3::QueueBase::MaxPackets", UintegerValue (50));
  
  // 4 MB of TCP buffer
  // Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 24));
  // Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 24));
  NodeContainer sources, sinks, attackers, routers;
  sources.Create(1);
  sinks.Create(1);
  attackers.Create(1);
  routers.Create(2);

  //const uint32_t bdp = 10 * 1e6 * 20 * 3 * 2 / 1000 / 8;
  PointToPointHelper LinkBottoleNeck;
  LinkBottoleNeck.SetDeviceAttribute("DataRate", StringValue("10Mbps")); // リンク帯域幅
  LinkBottoleNeck.SetChannelAttribute("Delay", StringValue("20ms"));     // リンクの片方向遅延
  //LinkBottoleNeck.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, 1000000)));
  LinkBottoleNeck.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, 50)));
  //LinkBottoleNeck.SetQueue("ns3::DropTailQueue", "Mode", EnumValue(QueueBase::QUEUE_MODE_PACKETS), "MaxPackets", UintegerValue(50));
  //LinkBottoleNeck.SetQueue("ns3::DropTailQueue", "Mode", EnumValue(QueueBase::QUEUE_MODE_BYTES), "MaxBytes", UintegerValue(bdp));

  PointToPointHelper Link100Mbps20ms;
  Link100Mbps20ms.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  Link100Mbps20ms.SetChannelAttribute("Delay", StringValue("20ms"));
  // LinkBottoleNeck.SetQueue("ns3::DropTailQueue", "Mode", EnumValue(QueueBase::QUEUE_MODE_BYTES), "MaxBytes", UintegerValue(bdp));
  //Link100Mbps20ms.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, 50)));

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  
  NetDeviceContainer devices;

  /* router 同士の接続 */
  devices = LinkBottoleNeck.Install(routers.Get(0), routers.Get(1));
  address.NewNetwork();
  address.Assign(devices);

  /* sourcesの接続 */
  for (uint32_t i = 0; i < sources.GetN(); i++)
  {    
    devices = Link100Mbps20ms.Install(sources.Get(i), routers.Get(0));
    address.NewNetwork();
    address.Assign(devices);    
  }

  /* attackersの接続 */
  for (uint32_t i = 0; i < attackers.GetN(); i++)
  {
    devices = Link100Mbps20ms.Install(attackers.Get(i), routers.Get(0));
    address.NewNetwork();
    address.Assign(devices);
  }

  /* sinksの接続 */
  // devices -> (routers_1)●-----●(sinks_0)
  devices = Link100Mbps20ms.Install(routers.Get(1), sinks.Get(0));
  address.NewNetwork();
  auto interface = address.Assign(devices);
  auto tcpSinkAddress = interface.Get(1);

  const int quic_sink_port = 443;
  const uint128_t bulk_send_max_bytes = 1 << 30;
  const double max_simu_time = 65.0;

  // TCPを送信する設定
  BulkSendHelper bulkSend("ns3::QuicSocketFactory", InetSocketAddress(interface.GetAddress(1), quic_sink_port));
  bulkSend.SetAttribute("MaxBytes", UintegerValue(bulk_send_max_bytes)); // MaxBytesに UintegerValue(bulk_send_max_bytes)を代入
  ApplicationContainer bulkSendApp = bulkSend.Install(sources.Get(0));   // sources[0]に
  bulkSendApp.Start(Seconds(0.0));
  bulkSendApp.Stop(Seconds(max_simu_time));

  // TCPを受信する設定
  PacketSinkHelper TCPsink("ns3::QuicSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), quic_sink_port));
  ApplicationContainer TCPSinkApp = TCPsink.Install(sinks.Get(0));
  TCPSinkApp.Start(Seconds(0.0)); // sinks[0]のノードがTCPを受信するアプリケーションの開始時刻を0秒に設定．シミュレーション開始後，すぐに開始
  TCPSinkApp.Stop(Seconds(max_simu_time));

  // UDP On-Off Application - Application used by attacker (eve) to create the low-rate bursts.
  bool shrew = true;
  const int udp_sink_port = 5000;
  const std::string attacker_rate = "62500kbps";//62500kbps
  const double attacker_start = 5.0; // シミュレーションで攻撃が開始する時間
  const float burst_period = 1.0;    // バースト間隔T
  const std::string on_time = "0.3"; // バースト長L
  const std::string off_time = std::to_string(burst_period - stof(on_time));

  if (shrew)
  {
    //NS_LOG_UNCOND("before");
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interface.GetAddress(1), udp_sink_port)));
    //NS_LOG_UNCOND("after");
    onoff.SetConstantRate(DataRate(attacker_rate), 50U);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + on_time + "]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + off_time + "]"));

    ApplicationContainer onOffApp = onoff.Install(attackers.Get(0));
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

  // make trace file's name
  std::string fname = "data/quic-minimum/tcp.throughput.csv";
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(fname);
  Simulator::Schedule(Seconds(0.1), &TraceThroughput, sinks.Get(0)->GetApplication(0), stream);

  LinkBottoleNeck.EnablePcapAll("data/quic-minimum/pcaps");

  /* Node info */
  NS_LOG_INFO("source: " << sources.Get(0)->GetId());
  NS_LOG_INFO("sink: " << sinks.Get(0)->GetId());
  NS_LOG_INFO("attacker: " << attackers.Get(0)->GetId());
  NS_LOG_INFO("Bottleneck router(0) : " << routers.Get(0)->GetId());
  NS_LOG_INFO("Bottleneck router(1) : " << routers.Get(1)->GetId());

  //NS_LOG_INFO("source: " << sources.Get(0)->GetApplication());
  //NS_LOG_INFO("sink: " << sinks.Get(0)->GetApplication());

  Simulator::Stop(Seconds(max_simu_time));
  Simulator::Run();

  NS_LOG_UNCOND("Simulation success 🎉");
  NS_LOG_UNCOND("------ Simulation Results ------");
  NS_LOG_UNCOND("Total TCP Transfer: " << std::to_string(newTotalBytes) << "Bytes");
  double th = (double)newTotalBytes * 8 / (max_simu_time - 1) / 1000 / 1000;
  NS_LOG_UNCOND("Throughput        : " << std::to_string(th) << "Mbps");

  Simulator::Destroy();
  return 0;
}