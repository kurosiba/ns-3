/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */


/*
シナリオファイルに必要なns3のモジュールをインクルードしてる。余分なものがあっても動く
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"


/*
シナリオファイルとモジュールは全てns3の名前空間に属しているから変更する必要はない
*/
using namespace ns3;

/*
特定の場所のログだけとりたいときに、そこに名前を付けるのがコレ、ロギング
*/
NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int
main (int argc, char *argv[])
{
/*
コマンドラインオプションを自作している
*/
  CommandLine cmd;
  cmd.Parse (argc, argv);
/*
シミュレーションの最小単位を1nsに設定しているとこ
*/
  Time::SetResolution (Time::NS);

/*
特定箇所のログを有効にしている
*/
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnableAll(LOG_PREFIX_NODE);
/*
ノードの設定
*/

/*
nodeのコンテナを作って、その中に使うコンテナを格納している
Ptr<Node> client = nodes.Get (0);のように取り出して定義できるし、
Ptr<Node> client = CreateObject<Node> ();のように名前を明示して定義できるし、
nodes.Add (client);のようにあとからnodeの追加もできる
*/
  NodeContainer nodes;
  nodes.Create (2);

/*
ネットデバイスとチャンネルの設定
*/

/*
1対1のトロポジを定義していて、"DataRate"というネットワークデバイスがもつAttribute(属性変数)、リンク速度を5Mbps。リンク遅延を2msと設定している
*/
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

/*
ネットデバイスのコンテナを作成し、トロポジヘルパーが勝手に割り当てをやってくれるようにしている
*/

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

/*
プロトコルスタックの設定をしていて、IPv4かIPv6かのどちらでやるかを設定している
*/

  InternetStackHelper stack;
  stack.Install (nodes);

/*
ネットワークアドレスやネットマスクを定義している
*/

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

/*
ネットワークアドレスやネットマスクをネットデバイスに設定している
*/

  Ipv4InterfaceContainer interfaces = address.Assign (devices);


/*
アプリケーション設定
*/

/*
クライアントからサーバへpingを飛ばせるように書いていく
*/

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

/*
シミュレーションの設定
*/
/*
これまでのシナリオの実行とシナリオ終了時にメモリを解放して終了することが書かれている
*/
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
