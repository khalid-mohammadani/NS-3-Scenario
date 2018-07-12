//2018-05-22

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-helper.h"
#include <iostream>
#include <cmath>
#include "ns3/wifi-phy.h"               //kh
#include "ns3/dsdv-packet.h"            //kh
#include "ns3/yans-wifi-phy.h"          //kh
#include "ns3/netanim-module.h"         //kh


using namespace ns3;



NS_LOG_COMPONENT_DEFINE ("DsdvManetExample");


uint32_t m_nodes=3; ///< total number of nodes
//uint32_t m_nSinks=1; ///< number of receiver nodes
double m_totalTime =10.0; ///< total simulation time (in seconds)
//std::string m_rate ("8kbps"); ///< network bandwidth
//std::string m_phyMode ("DsssRate11Mbps"); ///< remote station manager data mode
//uint32_t m_nodeSpeed = 1; ///< mobility speed in m/s
uint32_t m_periodicUpdateInterval = 3; ///< routing update interval
uint32_t m_settlingTime = 4; ///< routing setting time
double m_dataStart = 1.0; ///< time to start data transmissions (seconds)
uint32_t bytesTotal = 0; ///< total bytes received by all nodes
uint32_t packetsReceived = 0; ///< total packets received by all nodes
bool m_printRoutes = true; ///< print routing table
std::string m_CSVfileName = "DsdvManetExample.csv"; ///< CSV file name

NodeContainer nodes; ///< the collection of nodes
NetDeviceContainer devices; ///< the collection of devices
//Ipv4InterfaceContainer interfaces; ///< the collection of interfaces


MobilityHelper mobility;


//================================================================Mobility Model 
Ptr<MobilityModel>
GetMobilityModel (Ptr<Node> node) 
{
  mobility.Install(node);
  Ptr<Object> object = node;
  Ptr<MobilityModel> model = object->GetObject<MobilityModel> ();
  if (model == 0)
  {
      std::cout<<"Model is Null in GetMobilityModel() \n";
      std::exit(0);
  }
  return model;
}



void
ReceivePacket (Ptr <Socket> socket)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "Received one packet!"); 
  std::cout<<Simulator::Now ().GetSeconds () << "**************************--------------------------- Received one packet!"<<std::endl;  
  Ptr <Packet> packet;
  while ((packet = socket->Recv ()))
    {
      bytesTotal += packet->GetSize ();
      packetsReceived += 1;
    }
}


ApplicationContainer createConnection(uint32_t Source, uint32_t Sink, double startTime, double duration )
{
    uint16_t port = 9;
    //====================================================Install Sink Application 
    Ptr<Node> SourceNode = nodes.Get (Source); 
    Ptr<Node> SinkNode = nodes.Get (Sink); 
    
    Ipv4Address SinkNodeAddress = SinkNode->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();

    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    Ptr <Socket> sink = Socket::CreateSocket (SinkNode, tid);
    InetSocketAddress local = InetSocketAddress (SinkNodeAddress, port);
    sink->Bind (local);
    sink->SetRecvCallback (MakeCallback ( &ReceivePacket ));

    
    //=========================================== Install Source OnOff Application 
    
    OnOffHelper onoff1 ("ns3::UdpSocketFactory", Address (InetSocketAddress (SinkNodeAddress, port)));
    onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

    ApplicationContainer app = onoff1.Install (SourceNode);
//    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
//    app.Start (Seconds (var->GetValue (m_dataStart, m_dataStart + 1)));
//    app.Stop (Seconds (m_totalTime));

app.Start (Seconds (startTime));
app.Stop (Seconds (startTime+duration));

    return app;

}


Ptr<Node> createNode(const Vector &position, const WifiPhyHelper &phyHelper, const WifiMacHelper &wifiMac, const ObjectFactory &m_stationManager)
{
    nodes.Create (1);
    Ptr<Node> node = nodes.Get(nodes.GetN()-1);

    mobility.Install(node);
    Ptr<Object> object = node;
    Ptr<MobilityModel> model = object->GetObject<MobilityModel> ();
    //Ptr<MobilityModel> MobilityModel = node->GetObject<MobilityModel> ();
    model->SetPosition(position); 


    
    Ptr<WifiNetDevice> device = CreateObject<WifiNetDevice> ();
    
    Ptr<WifiRemoteStationManager> manager =   m_stationManager.Create<WifiRemoteStationManager> ();
    manager->SetDefaultTxPowerLevel(9);
    
    Ptr<WifiPhy> phy = phyHelper.Create (node, device);
    phy->ConfigureStandard (WIFI_PHY_STANDARD_80211b);
    
    phy->SetTxPowerStart (5.34);         // dbm = 15.60-> (700m) 
    phy->SetTxPowerEnd (21.72);          // 16.72 = 0.04698945 watts (755m)    
    phy->SetNTxPower (10);               //number of levels
    
   
    Ptr<WifiMac> mac = wifiMac.Create ();
    mac->SetAddress (Mac48Address::Allocate ());
    mac->ConfigureStandard (WIFI_PHY_STANDARD_80211b);
 
    device->SetMac (mac);
    device->SetPhy (phy);
    device->SetRemoteStationManager (manager);
    
    node->AddDevice (device);
    
    devices.Add (device);
   
    return node;
}



int main (int argc, char **argv)
{
  
  SeedManager::SetSeed (12345);

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue ("1024"));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("8kbps"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("DsssRate11Mbps"));
  
  ObjectFactory m_stationManager;
  m_stationManager = ObjectFactory ();
  m_stationManager.SetTypeId ("ns3::ArfWifiManager");
  
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  

  YansWifiChannelHelper wifiChannelHelper;
  wifiChannelHelper.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannelHelper.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  
   
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
  wifiPhyHelper.Set ("EnergyDetectionThreshold", DoubleValue(-88.0));	//dbm
  wifiPhyHelper.Set ("CcaMode1Threshold", DoubleValue(-91.0));	//dbm
  wifiPhyHelper.SetChannel (wifiChannelHelper.Create ());


  std::string tr_name = "Dsdv_Manet";
  
    
  
  //================================================================ CreateNodes
  //Nodes=8
//  createNode(Vector (300.0*3, 500.0*2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  createNode(Vector (200.0*3, 500.0*2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  createNode(Vector (100.0*3, 400.0*2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  createNode(Vector (100.0*3, 300.0*2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  createNode(Vector (200.0*3, 200.0*2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  createNode(Vector (300.0*3, 100.0*2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  createNode(Vector (400.0*3, 100.0*2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  createNode(Vector (500.0*3, 200.0*2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  

  
  for(int i=0; i<10; ++i)
  {
      int x1 = std::rand()%100+200;
      int y1 = std::rand()%100+200;
      
      int x2 = std::rand()%100+600;
      int y2 = std::rand()%100+600;
      
  createNode(Vector (x1, y1, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
  createNode(Vector (x2, y2, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
  
   }
  
  
  //createNode(Vector (100, 450, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
 
//  createNode(Vector (100, 500, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  createNode(Vector (800, 500, 0.0), wifiPhyHelper, wifiMac, m_stationManager);
//  
  
  
 //===================================Routing Protocol and InstallInternetStack
  DsdvHelper dsdv;
  dsdv.Set ("PeriodicUpdateInterval", TimeValue (Seconds (m_periodicUpdateInterval)));
  dsdv.Set ("SettlingTime", TimeValue (Seconds (m_settlingTime)));
 
  InternetStackHelper stack;
  stack.SetRoutingHelper (dsdv); // has effect on the next Install ()
  stack.Install (nodes);
  
  //====================================Assign IP address to Nodes 
  
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  //interfaces = address.Assign (devices);
  address.Assign (devices);
  
  //=========================================================================== 
  // create connection sourceNode, DestinationNode, StartTime, Duration
 // createConnection(0, 4, 1,20);
  createConnection(0, 19, 1,20);
  
          
  //======================================================= Starting Simulation

  //===== Tracing
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ((tr_name + ".routes"), std::ios::out);
  dsdv.PrintRoutingTableAllAt (Seconds (m_periodicUpdateInterval*3), routingStream);
  
  AsciiTraceHelper ascii;
  wifiPhyHelper.EnableAsciiAll (ascii.CreateFileStream (tr_name+".tr"));
 
  // wifiPhy.EnablePcapAll (tr_name);
  
  
  std::cout << "\nStarting simulation for " << m_totalTime << " s ...\n";
  
  AnimationInterface anim("khalid.xml"); //khalid
  
  Simulator::Stop (Seconds (m_totalTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
