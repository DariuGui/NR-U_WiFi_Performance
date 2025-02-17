/*
 * Copyright (c) 2022 Universita' degli Studi di Napoli Federico II
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
 * Author: Stefano Avallone <stavallo@unina.it>
 */

#include "ns3/ap-wifi-mac.h"
#include "ns3/config.h"
#include "ns3/he-configuration.h"
#include "ns3/he-frame-exchange-manager.h"
#include "ns3/he-phy.h"
#include "ns3/log.h"
#include "ns3/mgt-headers.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-link-element.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/node-list.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-server.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/qos-utils.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/spectrum-wifi-phy.h"
#include "ns3/sta-wifi-mac.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/wifi-acknowledgment.h"
#include "ns3/wifi-assoc-manager.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-protection.h"
#include "ns3/wifi-psdu.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMloTest");

/**
 * \ingroup wifi-test
 * \ingroup tests
 *
 * \brief Test the implementation of WifiAssocManager::GetNextAffiliatedAp(), which
 * searches a given RNR element for APs affiliated to the same AP MLD as the
 * reporting AP that sent the frame containing the element.
 */
class GetRnrLinkInfoTest : public TestCase
{
  public:
    /**
     * Constructor
     */
    GetRnrLinkInfoTest();
    ~GetRnrLinkInfoTest() override = default;

  private:
    void DoRun() override;
};

GetRnrLinkInfoTest::GetRnrLinkInfoTest()
    : TestCase("Check the implementation of WifiAssocManager::GetNextAffiliatedAp()")
{
}

void
GetRnrLinkInfoTest::DoRun()
{
    ReducedNeighborReport rnr;
    std::size_t nbrId;
    std::size_t tbttId;

    // Add a first Neighbor AP Information field without MLD Parameters
    rnr.AddNbrApInfoField();
    nbrId = rnr.GetNNbrApInfoFields() - 1;

    rnr.AddTbttInformationField(nbrId);
    rnr.AddTbttInformationField(nbrId);

    // Add a second Neighbor AP Information field with MLD Parameters; the first
    // TBTT Information field is related to an AP affiliated to the same AP MLD
    // as the reported AP; the second TBTT Information field is not (it does not
    // make sense that two APs affiliated to the same AP MLD are using the same
    // channel).
    rnr.AddNbrApInfoField();
    nbrId = rnr.GetNNbrApInfoFields() - 1;

    rnr.AddTbttInformationField(nbrId);
    tbttId = rnr.GetNTbttInformationFields(nbrId) - 1;
    rnr.SetMldParameters(nbrId, tbttId, 0, 0, 0);

    rnr.AddTbttInformationField(nbrId);
    tbttId = rnr.GetNTbttInformationFields(nbrId) - 1;
    rnr.SetMldParameters(nbrId, tbttId, 5, 0, 0);

    // Add a third Neighbor AP Information field with MLD Parameters; none of the
    // TBTT Information fields is related to an AP affiliated to the same AP MLD
    // as the reported AP.
    rnr.AddNbrApInfoField();
    nbrId = rnr.GetNNbrApInfoFields() - 1;

    rnr.AddTbttInformationField(nbrId);
    tbttId = rnr.GetNTbttInformationFields(nbrId) - 1;
    rnr.SetMldParameters(nbrId, tbttId, 3, 0, 0);

    rnr.AddTbttInformationField(nbrId);
    tbttId = rnr.GetNTbttInformationFields(nbrId) - 1;
    rnr.SetMldParameters(nbrId, tbttId, 4, 0, 0);

    // Add a fourth Neighbor AP Information field with MLD Parameters; the first
    // TBTT Information field is not related to an AP affiliated to the same AP MLD
    // as the reported AP; the second TBTT Information field is.
    rnr.AddNbrApInfoField();
    nbrId = rnr.GetNNbrApInfoFields() - 1;

    rnr.AddTbttInformationField(nbrId);
    tbttId = rnr.GetNTbttInformationFields(nbrId) - 1;
    rnr.SetMldParameters(nbrId, tbttId, 6, 0, 0);

    rnr.AddTbttInformationField(nbrId);
    tbttId = rnr.GetNTbttInformationFields(nbrId) - 1;
    rnr.SetMldParameters(nbrId, tbttId, 0, 0, 0);

    // check implementation of WifiAssocManager::GetNextAffiliatedAp()
    auto ret = WifiAssocManager::GetNextAffiliatedAp(rnr, 0);

    NS_TEST_EXPECT_MSG_EQ(ret.has_value(), true, "Expected to find a suitable reported AP");
    NS_TEST_EXPECT_MSG_EQ(ret->m_nbrApInfoId, 1, "Unexpected neighbor ID of the first reported AP");
    NS_TEST_EXPECT_MSG_EQ(ret->m_tbttInfoFieldId, 0, "Unexpected tbtt ID of the first reported AP");

    ret = WifiAssocManager::GetNextAffiliatedAp(rnr, ret->m_nbrApInfoId + 1);

    NS_TEST_EXPECT_MSG_EQ(ret.has_value(), true, "Expected to find a second suitable reported AP");
    NS_TEST_EXPECT_MSG_EQ(ret->m_nbrApInfoId,
                          3,
                          "Unexpected neighbor ID of the second reported AP");
    NS_TEST_EXPECT_MSG_EQ(ret->m_tbttInfoFieldId,
                          1,
                          "Unexpected tbtt ID of the second reported AP");

    ret = WifiAssocManager::GetNextAffiliatedAp(rnr, ret->m_nbrApInfoId + 1);

    NS_TEST_EXPECT_MSG_EQ(ret.has_value(),
                          false,
                          "Did not expect to find a third suitable reported AP");

    // check implementation of WifiAssocManager::GetAllAffiliatedAps()
    auto allAps = WifiAssocManager::GetAllAffiliatedAps(rnr);

    NS_TEST_EXPECT_MSG_EQ(allAps.size(), 2, "Expected to find two suitable reported APs");

    auto apIt = allAps.begin();
    NS_TEST_EXPECT_MSG_EQ(apIt->m_nbrApInfoId,
                          1,
                          "Unexpected neighbor ID of the first reported AP");
    NS_TEST_EXPECT_MSG_EQ(apIt->m_tbttInfoFieldId,
                          0,
                          "Unexpected tbtt ID of the first reported AP");

    apIt++;
    NS_TEST_EXPECT_MSG_EQ(apIt->m_nbrApInfoId,
                          3,
                          "Unexpected neighbor ID of the second reported AP");
    NS_TEST_EXPECT_MSG_EQ(apIt->m_tbttInfoFieldId,
                          1,
                          "Unexpected tbtt ID of the second reported AP");
}

/**
 * \ingroup wifi-test
 * \ingroup tests
 *
 * \brief Base class for Multi-Link Operations tests
 *
 * Three spectrum channels are created, one for each band (2.4 GHz, 5 GHz and 6 GHz).
 * Each PHY object is attached to the spectrum channel corresponding to the PHY band
 * in which it is operating.
 */
class MultiLinkOperationsTestBase : public TestCase
{
  public:
    /**
     * Constructor
     *
     * \param name The name of the new TestCase created
     * \param nStations the number of stations to create
     * \param staChannels the strings specifying the operating channels for the STA
     * \param apChannels the strings specifying the operating channels for the AP
     * \param fixedPhyBands list of IDs of STA links that cannot switch PHY band
     */
    MultiLinkOperationsTestBase(const std::string& name,
                                uint8_t nStations,
                                std::vector<std::string> staChannels,
                                std::vector<std::string> apChannels,
                                std::vector<uint8_t> fixedPhyBands = {});
    ~MultiLinkOperationsTestBase() override = default;

  protected:
    /**
     * Callback invoked when a FEM passes PSDUs to the PHY.
     *
     * \param linkId the ID of the link transmitting the PSDUs
     * \param context the context
     * \param psduMap the PSDU map
     * \param txVector the TX vector
     * \param txPowerW the tx power in Watts
     */
    virtual void Transmit(uint8_t linkId,
                          std::string context,
                          WifiConstPsduMap psduMap,
                          WifiTxVector txVector,
                          double txPowerW);

    void DoSetup() override;

    /**
     * Uplink or Downlink direction
     */
    enum Direction
    {
        DL = 0,
        UL
    };

    /**
     * Check that the Address 1 and Address 2 fields of the given PSDU contain device MAC addresses.
     *
     * \param psdu the given PSDU
     * \param direction indicates direction for management frames (DL or UL)
     */
    void CheckAddresses(Ptr<const WifiPsdu> psdu,
                        std::optional<Direction> direction = std::nullopt);

    /// Information about transmitted frames
    struct FrameInfo
    {
        Time startTx;             ///< TX start time
        WifiConstPsduMap psduMap; ///< transmitted PSDU map
        WifiTxVector txVector;    ///< TXVECTOR
        uint8_t linkId;           ///< link ID
    };

    std::vector<FrameInfo> m_txPsdus;             ///< transmitted PSDUs
    const std::vector<std::string> m_staChannels; ///< strings specifying channels for STA
    const std::vector<std::string> m_apChannels;  ///< strings specifying channels for AP
    const std::vector<uint8_t> m_fixedPhyBands;   ///< links on non-AP MLD with fixed PHY band
    Ptr<ApWifiMac> m_apMac;                       ///< AP wifi MAC
    std::vector<Ptr<StaWifiMac>> m_staMacs;       ///< STA wifi MACs
    uint8_t m_nStations;                          ///< number of stations to create
    uint16_t m_lastAid;                           ///< AID of last associated station

  private:
    /**
     * Reset the given PHY helper, use the given strings to set the ChannelSettings
     * attribute of the PHY objects to create, and attach them to the given spectrum
     * channels appropriately.
     *
     * \param helper the given PHY helper
     * \param channels the strings specifying the operating channels to configure
     * \param channel the created spectrum channel
     */
    void SetChannels(SpectrumWifiPhyHelper& helper,
                     const std::vector<std::string>& channels,
                     Ptr<MultiModelSpectrumChannel> channel);

    /**
     * Set the SSID on the next station that needs to start the association procedure.
     * This method is connected to the ApWifiMac's AssociatedSta trace source.
     * Start generating traffic (if needed) when all stations are associated.
     *
     * \param aid the AID assigned to the previous associated STA
     */
    void SetSsid(uint16_t aid, Mac48Address /* addr */);

    /**
     * Start the generation of traffic (needs to be overridden)
     */
    virtual void StartTraffic()
    {
    }
};

MultiLinkOperationsTestBase::MultiLinkOperationsTestBase(const std::string& name,
                                                         uint8_t nStations,
                                                         std::vector<std::string> staChannels,
                                                         std::vector<std::string> apChannels,
                                                         std::vector<uint8_t> fixedPhyBands)
    : TestCase(name),
      m_staChannels(staChannels),
      m_apChannels(apChannels),
      m_fixedPhyBands(fixedPhyBands),
      m_staMacs(nStations),
      m_nStations(nStations),
      m_lastAid(0)
{
}

void
MultiLinkOperationsTestBase::CheckAddresses(Ptr<const WifiPsdu> psdu,
                                            std::optional<Direction> direction)
{
    std::optional<Mac48Address> apAddr;
    std::optional<Mac48Address> staAddr;

    // direction for Data frames is derived from ToDS/FromDS flags
    if (psdu->GetHeader(0).IsQosData())
    {
        direction = (!psdu->GetHeader(0).IsToDs() && psdu->GetHeader(0).IsFromDs()) ? DL : UL;
    }
    NS_ASSERT(direction);

    if (direction == DL)
    {
        if (!psdu->GetAddr1().IsGroup())
        {
            staAddr = psdu->GetAddr1();
        }
        apAddr = psdu->GetAddr2();
    }
    else
    {
        if (!psdu->GetAddr1().IsGroup())
        {
            apAddr = psdu->GetAddr1();
        }
        staAddr = psdu->GetAddr2();
    }

    if (apAddr)
    {
        bool found = false;
        for (uint8_t linkId = 0; linkId < m_apMac->GetNLinks(); linkId++)
        {
            if (m_apMac->GetFrameExchangeManager(linkId)->GetAddress() == *apAddr)
            {
                found = true;
                break;
            }
        }
        NS_TEST_EXPECT_MSG_EQ(found,
                              true,
                              "Address " << *apAddr << " is not an AP device address. "
                                         << "PSDU: " << *psdu);
    }

    if (staAddr)
    {
        bool found = false;
        for (uint8_t i = 0; i < m_nStations; i++)
        {
            for (uint8_t linkId = 0; linkId < m_staMacs[i]->GetNLinks(); linkId++)
            {
                if (m_staMacs[i]->GetFrameExchangeManager(linkId)->GetAddress() == *staAddr)
                {
                    found = true;
                    break;
                }
            }
            if (found)
            {
                break;
            }
        }
        NS_TEST_EXPECT_MSG_EQ(found,
                              true,
                              "Address " << *staAddr << " is not a STA device address. "
                                         << "PSDU: " << *psdu);
    }
}

void
MultiLinkOperationsTestBase::Transmit(uint8_t linkId,
                                      std::string context,
                                      WifiConstPsduMap psduMap,
                                      WifiTxVector txVector,
                                      double txPowerW)
{
    m_txPsdus.push_back({Simulator::Now(), psduMap, txVector, linkId});

    std::stringstream ss;
    ss << std::setprecision(10) << "PSDU #" << m_txPsdus.size() << " Link ID " << +linkId << " "
       << psduMap.begin()->second->GetHeader(0).GetTypeString() << " #MPDUs "
       << psduMap.begin()->second->GetNMpdus() << " duration/ID "
       << psduMap.begin()->second->GetHeader(0).GetDuration()
       << " RA = " << psduMap.begin()->second->GetAddr1()
       << " TA = " << psduMap.begin()->second->GetAddr2()
       << " ADDR3 = " << psduMap.begin()->second->GetHeader(0).GetAddr3()
       << " ToDS = " << psduMap.begin()->second->GetHeader(0).IsToDs()
       << " FromDS = " << psduMap.begin()->second->GetHeader(0).IsFromDs();
    if (psduMap.begin()->second->GetHeader(0).IsQosData())
    {
        ss << " seqNo = {";
        for (auto& mpdu : *PeekPointer(psduMap.begin()->second))
        {
            ss << mpdu->GetHeader().GetSequenceNumber() << ",";
        }
        ss << "} TID = " << +psduMap.begin()->second->GetHeader(0).GetQosTid();
    }
    NS_LOG_INFO(ss.str());
    NS_LOG_INFO("TXVECTOR = " << txVector << "\n");
}

void
MultiLinkOperationsTestBase::SetChannels(SpectrumWifiPhyHelper& helper,
                                         const std::vector<std::string>& channels,
                                         Ptr<MultiModelSpectrumChannel> channel)
{
    helper = SpectrumWifiPhyHelper(channels.size());
    helper.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    uint8_t linkId = 0;
    for (const auto& str : channels)
    {
        helper.Set(linkId++, "ChannelSettings", StringValue(str));
    }

    helper.SetChannel(channel);
}

void
MultiLinkOperationsTestBase::DoSetup()
{
    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(2);
    int64_t streamNumber = 100;

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(m_nStations);

    WifiHelper wifi;
    // wifi.EnableLogComponents ();
    wifi.SetStandard(WIFI_STANDARD_80211be);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("EhtMcs0"));

    auto channel = CreateObject<MultiModelSpectrumChannel>();

    SpectrumWifiPhyHelper staPhyHelper;
    SpectrumWifiPhyHelper apPhyHelper;
    SetChannels(staPhyHelper, m_staChannels, channel);
    SetChannels(apPhyHelper, m_apChannels, channel);

    for (const auto& linkId : m_fixedPhyBands)
    {
        staPhyHelper.Set(linkId, "FixedPhyBand", BooleanValue(true));
    }

    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac", // default SSID
                "ActiveProbing",
                BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(staPhyHelper, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid",
                SsidValue(Ssid("ns-3-ssid")),
                "BeaconGeneration",
                BooleanValue(true));

    NetDeviceContainer apDevices = wifi.Install(apPhyHelper, mac, wifiApNode);

    // Uncomment the lines below to write PCAP files
    // apPhyHelper.EnablePcap("wifi-mlo_AP", apDevices);
    // staPhyHelper.EnablePcap("wifi-mlo_STA", staDevices);

    // Assign fixed streams to random variables in use
    streamNumber += wifi.AssignStreams(apDevices, streamNumber);
    streamNumber += wifi.AssignStreams(staDevices, streamNumber);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(1.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    m_apMac = DynamicCast<ApWifiMac>(DynamicCast<WifiNetDevice>(apDevices.Get(0))->GetMac());
    for (uint8_t i = 0; i < m_nStations; i++)
    {
        m_staMacs[i] =
            DynamicCast<StaWifiMac>(DynamicCast<WifiNetDevice>(staDevices.Get(i))->GetMac());
    }

    // Trace PSDUs passed to the PHY on all devices
    for (uint8_t linkId = 0; linkId < StaticCast<WifiNetDevice>(apDevices.Get(0))->GetNPhys();
         linkId++)
    {
        Config::Connect("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phys/" +
                            std::to_string(linkId) + "/PhyTxPsduBegin",
                        MakeCallback(&MultiLinkOperationsTestBase::Transmit, this).Bind(linkId));
    }
    for (uint8_t i = 0; i < m_nStations; i++)
    {
        for (uint8_t linkId = 0; linkId < StaticCast<WifiNetDevice>(staDevices.Get(i))->GetNPhys();
             linkId++)
        {
            Config::Connect(
                "/NodeList/" + std::to_string(i + 1) + "/DeviceList/*/$ns3::WifiNetDevice/Phys/" +
                    std::to_string(linkId) + "/PhyTxPsduBegin",
                MakeCallback(&MultiLinkOperationsTestBase::Transmit, this).Bind(linkId));
        }
    }

    // schedule ML setup for one station at a time
    m_apMac->TraceConnectWithoutContext("AssociatedSta",
                                        MakeCallback(&MultiLinkOperationsTestBase::SetSsid, this));
    Simulator::Schedule(Seconds(0), [&]() { m_staMacs[0]->SetSsid(Ssid("ns-3-ssid")); });
}

void
MultiLinkOperationsTestBase::SetSsid(uint16_t aid, Mac48Address /* addr */)
{
    if (m_lastAid == aid)
    {
        // another STA of this non-AP MLD has already fired this callback
        return;
    }
    m_lastAid = aid;

    // make the next STA to start ML discovery & setup
    if (aid < m_nStations)
    {
        m_staMacs[aid]->SetSsid(Ssid("ns-3-ssid"));
        return;
    }
    // wait some time (5ms) to allow the completion of association before generating traffic
    Simulator::Schedule(MilliSeconds(5), &MultiLinkOperationsTestBase::StartTraffic, this);
}

/**
 * \ingroup wifi-test
 * \ingroup tests
 *
 * \brief Multi-Link Discovery & Setup test.
 *
 * This test sets up an AP MLD and a non-AP MLD having a variable number of links.
 * The RF channels to set each link to are provided as input parameters through the test
 * case constructor, along with the identifiers (starting at 0) of the links that cannot
 * switch PHY band (if any). The links that are expected to be setup are also provided as input
 * parameters. This test verifies that the management frames exchanged during ML discovery
 * and ML setup contain the expected values and that the two MLDs setup the expected links.
 */
class MultiLinkSetupTest : public MultiLinkOperationsTestBase
{
  public:
    /**
     * Constructor
     *
     * \param staChannels the strings specifying the operating channels for the STA
     * \param apChannels the strings specifying the operating channels for the AP
     * \param setupLinks a list of links (STA link ID, AP link ID) that are expected to be setup
     * \param fixedPhyBands list of IDs of STA links that cannot switch PHY band
     */
    MultiLinkSetupTest(std::vector<std::string> staChannels,
                       std::vector<std::string> apChannels,
                       std::vector<std::pair<uint8_t, uint8_t>> setupLinks,
                       std::vector<uint8_t> fixedPhyBands = {});
    ~MultiLinkSetupTest() override = default;

  protected:
    void DoRun() override;

  private:
    /**
     * Check correctness of Multi-Link Setup procedure.
     */
    void CheckMlSetup();

    /**
     * Check that links that are not setup on the non-AP MLD are disabled.
     */
    void CheckDisabledLinks();

    /**
     * Check correctness of the given Beacon frame.
     *
     * \param mpdu the given Beacon frame
     * \param linkId the ID of the link on which the Beacon frame was transmitted
     */
    void CheckBeacon(Ptr<WifiMpdu> mpdu, uint8_t linkId);

    /**
     * Check correctness of the given Association Request frame.
     *
     * \param mpdu the given Association Request frame
     * \param linkId the ID of the link on which the Association Request frame was transmitted
     */
    void CheckAssocRequest(Ptr<WifiMpdu> mpdu, uint8_t linkId);

    /**
     * Check correctness of the given Association Response frame.
     *
     * \param mpdu the given Association Response frame
     * \param linkId the ID of the link on which the Association Response frame was transmitted
     */
    void CheckAssocResponse(Ptr<WifiMpdu> mpdu, uint8_t linkId);

    /// expected links to setup (STA link ID, AP link ID)
    const std::vector<std::pair<uint8_t, uint8_t>> m_setupLinks;
};

MultiLinkSetupTest::MultiLinkSetupTest(std::vector<std::string> staChannels,
                                       std::vector<std::string> apChannels,
                                       std::vector<std::pair<uint8_t, uint8_t>> setupLinks,
                                       std::vector<uint8_t> fixedPhyBands)
    : MultiLinkOperationsTestBase("Check correctness of Multi-Link Setup",
                                  1,
                                  staChannels,
                                  apChannels,
                                  fixedPhyBands),
      m_setupLinks(setupLinks)
{
}

void
MultiLinkSetupTest::DoRun()
{
    Simulator::Schedule(MilliSeconds(500), &MultiLinkSetupTest::CheckMlSetup, this);

    Simulator::Stop(Seconds(1.5));
    Simulator::Run();

    /**
     * Check content of management frames
     */
    for (const auto& frameInfo : m_txPsdus)
    {
        const auto& mpdu = *frameInfo.psduMap.begin()->second->begin();
        const auto& linkId = frameInfo.linkId;

        switch (mpdu->GetHeader().GetType())
        {
        case WIFI_MAC_MGT_BEACON:
            CheckBeacon(mpdu, linkId);
            break;

        case WIFI_MAC_MGT_ASSOCIATION_REQUEST:
            CheckAssocRequest(mpdu, linkId);
            break;

        case WIFI_MAC_MGT_ASSOCIATION_RESPONSE:
            CheckAssocResponse(mpdu, linkId);
            break;

        default:
            break;
        }
    }

    CheckDisabledLinks();

    Simulator::Destroy();
}

void
MultiLinkSetupTest::CheckBeacon(Ptr<WifiMpdu> mpdu, uint8_t linkId)
{
    NS_ABORT_IF(mpdu->GetHeader().GetType() != WIFI_MAC_MGT_BEACON);

    CheckAddresses(Create<WifiPsdu>(mpdu, false), MultiLinkOperationsTestBase::DL);

    NS_TEST_EXPECT_MSG_EQ(m_apMac->GetFrameExchangeManager(linkId)->GetAddress(),
                          mpdu->GetHeader().GetAddr2(),
                          "TA of Beacon frame is not the address of the link it is transmitted on");
    MgtBeaconHeader beacon;
    mpdu->GetPacket()->PeekHeader(beacon);
    const auto& rnr = beacon.GetReducedNeighborReport();
    const auto& mle = beacon.GetMultiLinkElement();

    if (m_apMac->GetNLinks() == 1)
    {
        NS_TEST_EXPECT_MSG_EQ(rnr.has_value(),
                              false,
                              "RNR Element in Beacon frame from single link AP");
        NS_TEST_EXPECT_MSG_EQ(mle.has_value(),
                              false,
                              "Multi-Link Element in Beacon frame from single link AP");
        return;
    }

    NS_TEST_EXPECT_MSG_EQ(rnr.has_value(), true, "No RNR Element in Beacon frame");
    // All the other APs affiliated with the same AP MLD as the AP sending
    // the Beacon frame must be reported in a separate Neighbor AP Info field
    NS_TEST_EXPECT_MSG_EQ(rnr->GetNNbrApInfoFields(),
                          static_cast<std::size_t>(m_apMac->GetNLinks() - 1),
                          "Unexpected number of Neighbor AP Info fields in RNR");
    for (std::size_t nbrApInfoId = 0; nbrApInfoId < rnr->GetNNbrApInfoFields(); nbrApInfoId++)
    {
        NS_TEST_EXPECT_MSG_EQ(rnr->HasMldParameters(nbrApInfoId),
                              true,
                              "MLD Parameters not present");
        NS_TEST_EXPECT_MSG_EQ(rnr->GetNTbttInformationFields(nbrApInfoId),
                              1,
                              "Expected only one TBTT Info subfield per Neighbor AP Info");
        uint8_t nbrLinkId = rnr->GetLinkId(nbrApInfoId, 0);
        NS_TEST_EXPECT_MSG_EQ(rnr->GetBssid(nbrApInfoId, 0),
                              m_apMac->GetFrameExchangeManager(nbrLinkId)->GetAddress(),
                              "BSSID advertised in Neighbor AP Info field "
                                  << nbrApInfoId
                                  << " does not match the address configured on the link "
                                     "advertised in the same field");
    }

    NS_TEST_EXPECT_MSG_EQ(mle.has_value(), true, "No Multi-Link Element in Beacon frame");
    NS_TEST_EXPECT_MSG_EQ(mle->GetMldMacAddress(),
                          m_apMac->GetAddress(),
                          "Incorrect MLD address advertised in Multi-Link Element");
    NS_TEST_EXPECT_MSG_EQ(mle->GetLinkIdInfo(),
                          +linkId,
                          "Incorrect Link ID advertised in Multi-Link Element");
}

void
MultiLinkSetupTest::CheckAssocRequest(Ptr<WifiMpdu> mpdu, uint8_t linkId)
{
    NS_ABORT_IF(mpdu->GetHeader().GetType() != WIFI_MAC_MGT_ASSOCIATION_REQUEST);

    CheckAddresses(Create<WifiPsdu>(mpdu, false), MultiLinkOperationsTestBase::UL);

    NS_TEST_EXPECT_MSG_EQ(
        m_staMacs[0]->GetFrameExchangeManager(linkId)->GetAddress(),
        mpdu->GetHeader().GetAddr2(),
        "TA of Assoc Request frame is not the address of the link it is transmitted on");
    MgtAssocRequestHeader assoc;
    mpdu->GetPacket()->PeekHeader(assoc);
    const auto& mle = assoc.GetMultiLinkElement();

    if (m_apMac->GetNLinks() == 1 || m_staMacs[0]->GetNLinks() == 1)
    {
        NS_TEST_EXPECT_MSG_EQ(mle.has_value(),
                              false,
                              "Multi-Link Element in Assoc Request frame from single link STA");
        return;
    }

    NS_TEST_EXPECT_MSG_EQ(mle.has_value(), true, "No Multi-Link Element in Assoc Request frame");
    NS_TEST_EXPECT_MSG_EQ(mle->GetMldMacAddress(),
                          m_staMacs[0]->GetAddress(),
                          "Incorrect MLD Address advertised in Multi-Link Element");
    NS_TEST_EXPECT_MSG_EQ(mle->GetNPerStaProfileSubelements(),
                          m_setupLinks.size() - 1,
                          "Incorrect number of Per-STA Profile subelements in Multi-Link Element");
    for (std::size_t i = 0; i < mle->GetNPerStaProfileSubelements(); i++)
    {
        auto& perStaProfile = mle->GetPerStaProfile(i);
        NS_TEST_EXPECT_MSG_EQ(perStaProfile.HasStaMacAddress(),
                              true,
                              "Per-STA Profile must contain STA MAC address");
        // find ID of the local link corresponding to this subelement
        auto staLinkId = m_staMacs[0]->GetLinkIdByAddress(perStaProfile.GetStaMacAddress());
        NS_TEST_EXPECT_MSG_EQ(
            staLinkId.has_value(),
            true,
            "No link found with the STA MAC address advertised in Per-STA Profile");
        NS_TEST_EXPECT_MSG_NE(
            +staLinkId.value(),
            +linkId,
            "The STA that sent the Assoc Request should not be included in a Per-STA Profile");
        auto it = std::find_if(m_setupLinks.begin(), m_setupLinks.end(), [&staLinkId](auto&& pair) {
            return pair.first == staLinkId.value();
        });
        NS_TEST_EXPECT_MSG_EQ((it != m_setupLinks.end()),
                              true,
                              "Not expecting to setup STA link ID " << +staLinkId.value());
        NS_TEST_EXPECT_MSG_EQ(
            +it->second,
            +perStaProfile.GetLinkId(),
            "Not expecting to request association to AP Link ID in Per-STA Profile");
        NS_TEST_EXPECT_MSG_EQ(perStaProfile.HasAssocRequest(),
                              true,
                              "Missing Association Request in Per-STA Profile");
    }
}

void
MultiLinkSetupTest::CheckAssocResponse(Ptr<WifiMpdu> mpdu, uint8_t linkId)
{
    NS_ABORT_IF(mpdu->GetHeader().GetType() != WIFI_MAC_MGT_ASSOCIATION_RESPONSE);

    CheckAddresses(Create<WifiPsdu>(mpdu, false), MultiLinkOperationsTestBase::DL);

    NS_TEST_EXPECT_MSG_EQ(
        m_apMac->GetFrameExchangeManager(linkId)->GetAddress(),
        mpdu->GetHeader().GetAddr2(),
        "TA of Assoc Response frame is not the address of the link it is transmitted on");
    MgtAssocResponseHeader assoc;
    mpdu->GetPacket()->PeekHeader(assoc);
    const auto& mle = assoc.GetMultiLinkElement();

    if (m_apMac->GetNLinks() == 1 || m_staMacs[0]->GetNLinks() == 1)
    {
        NS_TEST_EXPECT_MSG_EQ(
            mle.has_value(),
            false,
            "Multi-Link Element in Assoc Response frame with single link AP or single link STA");
        return;
    }

    NS_TEST_EXPECT_MSG_EQ(mle.has_value(), true, "No Multi-Link Element in Assoc Request frame");
    NS_TEST_EXPECT_MSG_EQ(mle->GetMldMacAddress(),
                          m_apMac->GetAddress(),
                          "Incorrect MLD Address advertised in Multi-Link Element");
    NS_TEST_EXPECT_MSG_EQ(mle->GetNPerStaProfileSubelements(),
                          m_setupLinks.size() - 1,
                          "Incorrect number of Per-STA Profile subelements in Multi-Link Element");
    for (std::size_t i = 0; i < mle->GetNPerStaProfileSubelements(); i++)
    {
        auto& perStaProfile = mle->GetPerStaProfile(i);
        NS_TEST_EXPECT_MSG_EQ(perStaProfile.HasStaMacAddress(),
                              true,
                              "Per-STA Profile must contain STA MAC address");
        // find ID of the local link corresponding to this subelement
        auto apLinkId = m_apMac->GetLinkIdByAddress(perStaProfile.GetStaMacAddress());
        NS_TEST_EXPECT_MSG_EQ(
            apLinkId.has_value(),
            true,
            "No link found with the STA MAC address advertised in Per-STA Profile");
        NS_TEST_EXPECT_MSG_EQ(+apLinkId.value(),
                              +perStaProfile.GetLinkId(),
                              "Link ID and MAC address advertised in Per-STA Profile do not match");
        NS_TEST_EXPECT_MSG_NE(
            +apLinkId.value(),
            +linkId,
            "The AP that sent the Assoc Response should not be included in a Per-STA Profile");
        auto it = std::find_if(m_setupLinks.begin(), m_setupLinks.end(), [&apLinkId](auto&& pair) {
            return pair.second == apLinkId.value();
        });
        NS_TEST_EXPECT_MSG_EQ((it != m_setupLinks.end()),
                              true,
                              "Not expecting to setup AP link ID " << +apLinkId.value());
        NS_TEST_EXPECT_MSG_EQ(perStaProfile.HasAssocResponse(),
                              true,
                              "Missing Association Response in Per-STA Profile");
    }
}

void
MultiLinkSetupTest::CheckMlSetup()
{
    /**
     * Check outcome of Multi-Link Setup
     */
    NS_TEST_EXPECT_MSG_EQ(m_staMacs[0]->IsAssociated(), true, "Expected the STA to be associated");

    for (const auto& [staLinkId, apLinkId] : m_setupLinks)
    {
        auto staAddr = m_staMacs[0]->GetFrameExchangeManager(staLinkId)->GetAddress();
        auto apAddr = m_apMac->GetFrameExchangeManager(apLinkId)->GetAddress();

        auto staRemoteMgr = m_staMacs[0]->GetWifiRemoteStationManager(staLinkId);
        auto apRemoteMgr = m_apMac->GetWifiRemoteStationManager(apLinkId);

        // STA side
        NS_TEST_EXPECT_MSG_EQ(m_staMacs[0]->GetFrameExchangeManager(staLinkId)->GetBssid(),
                              apAddr,
                              "Unexpected BSSID for STA link ID " << +staLinkId);
        if (m_apMac->GetNLinks() > 1 && m_staMacs[0]->GetNLinks() > 1)
        {
            NS_TEST_EXPECT_MSG_EQ((staRemoteMgr->GetMldAddress(apAddr) == m_apMac->GetAddress()),
                                  true,
                                  "Incorrect MLD address stored by STA on link ID " << +staLinkId);
            NS_TEST_EXPECT_MSG_EQ(
                (staRemoteMgr->GetAffiliatedStaAddress(m_apMac->GetAddress()) == apAddr),
                true,
                "Incorrect affiliated address stored by STA on link ID " << +staLinkId);
        }

        // AP side
        NS_TEST_EXPECT_MSG_EQ(apRemoteMgr->IsAssociated(staAddr),
                              true,
                              "Expecting STA " << staAddr << " to be associated on link "
                                               << +apLinkId);
        if (m_apMac->GetNLinks() > 1 && m_staMacs[0]->GetNLinks() > 1)
        {
            NS_TEST_EXPECT_MSG_EQ(
                (apRemoteMgr->GetMldAddress(staAddr) == m_staMacs[0]->GetAddress()),
                true,
                "Incorrect MLD address stored by AP on link ID " << +apLinkId);
            NS_TEST_EXPECT_MSG_EQ(
                (apRemoteMgr->GetAffiliatedStaAddress(m_staMacs[0]->GetAddress()) == staAddr),
                true,
                "Incorrect affiliated address stored by AP on link ID " << +apLinkId);
        }
        auto aid = m_apMac->GetAssociationId(staAddr, apLinkId);
        const auto& staList = m_apMac->GetStaList(apLinkId);
        NS_TEST_EXPECT_MSG_EQ((staList.find(aid) != staList.end()),
                              true,
                              "STA " << staAddr << " not found in list of associated STAs");

        // STA of non-AP MLD operate on the same channel as the AP
        NS_TEST_EXPECT_MSG_EQ(
            +m_staMacs[0]->GetWifiPhy(staLinkId)->GetOperatingChannel().GetNumber(),
            +m_apMac->GetWifiPhy(apLinkId)->GetOperatingChannel().GetNumber(),
            "Incorrect operating channel number for STA on link " << +staLinkId);
        NS_TEST_EXPECT_MSG_EQ(
            m_staMacs[0]->GetWifiPhy(staLinkId)->GetOperatingChannel().GetFrequency(),
            m_apMac->GetWifiPhy(apLinkId)->GetOperatingChannel().GetFrequency(),
            "Incorrect operating channel frequency for STA on link " << +staLinkId);
        NS_TEST_EXPECT_MSG_EQ(m_staMacs[0]->GetWifiPhy(staLinkId)->GetOperatingChannel().GetWidth(),
                              m_apMac->GetWifiPhy(apLinkId)->GetOperatingChannel().GetWidth(),
                              "Incorrect operating channel width for STA on link " << +staLinkId);
        NS_TEST_EXPECT_MSG_EQ(
            +m_staMacs[0]->GetWifiPhy(staLinkId)->GetOperatingChannel().GetPhyBand(),
            +m_apMac->GetWifiPhy(apLinkId)->GetOperatingChannel().GetPhyBand(),
            "Incorrect operating PHY band for STA on link " << +staLinkId);
        NS_TEST_EXPECT_MSG_EQ(
            +m_staMacs[0]->GetWifiPhy(staLinkId)->GetOperatingChannel().GetPrimaryChannelIndex(20),
            +m_apMac->GetWifiPhy(apLinkId)->GetOperatingChannel().GetPrimaryChannelIndex(20),
            "Incorrect operating primary channel index for STA on link " << +staLinkId);
    }
}

void
MultiLinkSetupTest::CheckDisabledLinks()
{
    for (std::size_t linkId = 0; linkId < m_staChannels.size(); linkId++)
    {
        auto it = std::find_if(m_setupLinks.begin(), m_setupLinks.end(), [&linkId](auto&& link) {
            return link.first == linkId;
        });
        if (it == m_setupLinks.end())
        {
            // the link has not been setup
            NS_TEST_EXPECT_MSG_EQ(m_staMacs[0]->GetWifiPhy(linkId)->GetState()->IsStateOff(),
                                  true,
                                  "Link " << +linkId << " has not been setup but is not disabled");
            continue;
        }

        // the link has been setup and must be active
        NS_TEST_EXPECT_MSG_EQ(m_staMacs[0]->GetWifiPhy(linkId)->GetState()->IsStateOff(),
                              false,
                              "Expecting link " << +linkId << " to be active");
    }
}

/**
 * Tested traffic patterns.
 */
enum class WifiTrafficPattern : uint8_t
{
    STA_TO_STA = 0,
    STA_TO_AP,
    AP_TO_STA,
    AP_TO_BCAST,
    STA_TO_BCAST
};

/**
 * Block Ack agreement enabled/disabled
 */
enum class WifiBaEnabled : uint8_t
{
    NO = 0,
    YES
};

/**
 * \ingroup wifi-test
 * \ingroup tests
 *
 * \brief Test data transmission between two MLDs.
 *
 * This test sets up an AP MLD and two non-AP MLDs having a variable number of links.
 * The RF channels to set each link to are provided as input parameters through the test
 * case constructor, along with the identifiers (starting at 0) of the links that cannot
 * switch PHY band (if any). This test aims at veryfing the successful transmission of both
 * unicast QoS data frames (from one station to another, from one station to the AP, from
 * the AP to the station) and broadcast QoS data frames (from the AP or from one station).
 * In the scenarios in which the AP forwards frames (i.e., from one station to another and
 * from one station to broadcast) the client application generates only 4 packets, in order
 * to limit the probability of collisions. In the other scenarios, 8 packets are generated.
 * When BlockAck agreements are enabled, the maximum A-MSDU size is set such that two
 * packets can be aggregated in an A-MSDU. The MPDU with sequence number equal to 1 is
 * corrupted (once, by using a post reception error model) to test its successful
 * re-transmission, unless the traffic scenario is from the AP to broadcast (broadcast frames
 * are not retransmitted) or is a scenario where the AP forwards frame (to limit the
 * probability of collisions).
 */
class MultiLinkTxTest : public MultiLinkOperationsTestBase
{
  public:
    /**
     * Constructor
     *
     * \param trafficPattern the pattern of traffic to generate
     * \param baEnabled whether BA agreement is enabled or disabled
     * \param nMaxInflight the max number of links on which an MPDU can be simultaneously inflight
     *                     (unused if Block Ack agreements are not established)
     * \param staChannels the strings specifying the operating channels for the STA
     * \param apChannels the strings specifying the operating channels for the AP
     * \param fixedPhyBands list of IDs of STA links that cannot switch PHY band
     */
    MultiLinkTxTest(WifiTrafficPattern trafficPattern,
                    WifiBaEnabled baEnabled,
                    uint8_t nMaxInflight,
                    const std::vector<std::string>& staChannels,
                    const std::vector<std::string>& apChannels,
                    const std::vector<uint8_t>& fixedPhyBands = {});
    ~MultiLinkTxTest() override = default;

  protected:
    /**
     * Function to trace packets received by the server application
     * \param nodeId the ID of the node that received the packet
     * \param p the packet
     * \param addr the address
     */
    void L7Receive(uint8_t nodeId, Ptr<const Packet> p, const Address& addr);

    /**
     * Check the content of a received BlockAck frame when the max number of links on which
     * an MPDU can be inflight is one.
     *
     * \param psdu the PSDU containing the BlockAck
     * \param txVector the TXVECTOR used to transmit the BlockAck
     * \param linkId the ID of the link on which the BlockAck was transmitted
     */
    void CheckBlockAck(Ptr<const WifiPsdu> psdu, const WifiTxVector& txVector, uint8_t linkId);

    void Transmit(uint8_t linkId,
                  std::string context,
                  WifiConstPsduMap psduMap,
                  WifiTxVector txVector,
                  double txPowerW) override;
    void DoSetup() override;
    void DoRun() override;

  private:
    void StartTraffic() override;

    /// Receiver address-indexed map of list error models
    using RxErrorModelMap = std::unordered_map<Mac48Address, Ptr<ListErrorModel>, WifiAddressHash>;

    RxErrorModelMap m_errorModels;         ///< error rate models to corrupt packets
    std::list<uint64_t> m_uidList;         ///< list of UIDs of packets to corrupt
    bool m_dataCorrupted{false};           ///< whether second data frame has been already corrupted
    WifiTrafficPattern m_trafficPattern;   ///< the pattern of traffic to generate
    bool m_baEnabled;                      ///< whether BA agreement is enabled or disabled
    std::size_t m_nMaxInflight;            ///< max number of links on which an MPDU can be inflight
    std::size_t m_nPackets;                ///< number of application packets to generate
    std::size_t m_blockAckCount{0};        ///< transmitted BlockAck counter
    std::array<std::size_t, 3> m_rxPkts{}; ///< number of packets received at application layer
                                           ///< by each node (AP, STA 0, STA 1)
    std::map<uint16_t, std::size_t> m_inflightCount; ///< seqNo-indexed max number of simultaneous
                                                     ///< transmissions of a data frame
    Ptr<WifiMac> m_sourceMac; ///< MAC of the node sending application packets
};

MultiLinkTxTest::MultiLinkTxTest(WifiTrafficPattern trafficPattern,
                                 WifiBaEnabled baEnabled,
                                 uint8_t nMaxInflight,
                                 const std::vector<std::string>& staChannels,
                                 const std::vector<std::string>& apChannels,
                                 const std::vector<uint8_t>& fixedPhyBands)
    : MultiLinkOperationsTestBase(std::string("Check data transmission between MLDs ") +
                                      (baEnabled == WifiBaEnabled::YES ? "with" : "without") +
                                      " BA agreement (Traffic pattern: " +
                                      std::to_string(static_cast<uint8_t>(trafficPattern)) +
                                      (baEnabled == WifiBaEnabled::YES
                                           ? ", nMaxInflight=" + std::to_string(nMaxInflight)
                                           : "") +
                                      ")",
                                  2,
                                  staChannels,
                                  apChannels,
                                  fixedPhyBands),
      m_trafficPattern(trafficPattern),
      m_baEnabled(baEnabled == WifiBaEnabled::YES),
      m_nMaxInflight(nMaxInflight),
      m_nPackets(trafficPattern == WifiTrafficPattern::STA_TO_BCAST ||
                         trafficPattern == WifiTrafficPattern::STA_TO_STA
                     ? 4
                     : 8)
{
}

void
MultiLinkTxTest::Transmit(uint8_t linkId,
                          std::string context,
                          WifiConstPsduMap psduMap,
                          WifiTxVector txVector,
                          double txPowerW)
{
    auto psdu = psduMap.begin()->second;

    switch (psdu->GetHeader(0).GetType())
    {
    case WIFI_MAC_MGT_ACTION:
        // a management frame is a DL frame if TA equals BSSID
        CheckAddresses(psdu,
                       psdu->GetHeader(0).GetAddr2() == psdu->GetHeader(0).GetAddr3() ? DL : UL);
        if (!m_baEnabled)
        {
            // corrupt all management action frames (ADDBA Request frames) to prevent
            // the establishment of a BA agreement
            m_uidList.push_front(psdu->GetPacket()->GetUid());
            m_errorModels.at(psdu->GetAddr1())->SetList(m_uidList);
            NS_LOG_INFO("CORRUPTED");
        }
        break;
    case WIFI_MAC_QOSDATA:
        CheckAddresses(psdu);

        for (const auto& mpdu : *psdu)
        {
            // determine the max number of simultaneous transmissions for this MPDU
            // (only if sent by the traffic source and this is not a broadcast frame)
            if (m_baEnabled && linkId < m_sourceMac->GetNLinks() &&
                m_sourceMac->GetFrameExchangeManager(linkId)->GetAddress() ==
                    mpdu->GetHeader().GetAddr2() &&
                !mpdu->GetHeader().GetAddr1().IsGroup())
            {
                auto seqNo = mpdu->GetHeader().GetSequenceNumber();
                auto [it, success] =
                    m_inflightCount.insert({seqNo, mpdu->GetInFlightLinkIds().size()});
                if (!success)
                {
                    it->second = std::max(it->second, mpdu->GetInFlightLinkIds().size());
                }
            }
        }
        for (std::size_t i = 0; i < psdu->GetNMpdus(); i++)
        {
            // corrupt QoS data frame with sequence number equal to 1 (only once) if we are
            // not in the AP to broadcast traffic pattern (broadcast frames are not retransmitted)
            // nor in the STA to broadcast or STA to STA traffic patterns (retransmissions from
            // STA 1 could collide with frames forwarded by the AP)
            if (psdu->GetHeader(i).GetSequenceNumber() != 1 ||
                m_trafficPattern == WifiTrafficPattern::AP_TO_BCAST ||
                m_trafficPattern == WifiTrafficPattern::STA_TO_BCAST ||
                m_trafficPattern == WifiTrafficPattern::STA_TO_STA)
            {
                continue;
            }
            auto uid = psdu->GetPayload(i)->GetUid();
            if (!m_dataCorrupted)
            {
                m_uidList.push_front(uid);
                m_dataCorrupted = true;
                NS_LOG_INFO("CORRUPTED");
                m_errorModels.at(psdu->GetAddr1())->SetList(m_uidList);
            }
            else
            {
                // do not corrupt the QoS data frame anymore
                if (auto it = std::find(m_uidList.cbegin(), m_uidList.cend(), uid);
                    it != m_uidList.cend())
                {
                    m_uidList.erase(it);
                }
                m_errorModels.at(psdu->GetAddr1())->SetList(m_uidList);
            }
            break;
        }
        break;
    case WIFI_MAC_CTL_BACKRESP: {
        // ignore BlockAck frames not addressed to the source of the application packets
        if (!m_sourceMac->GetLinkIdByAddress(psdu->GetHeader(0).GetAddr1()))
        {
            break;
        }
        if (m_nMaxInflight > 1)
        {
            // we do not check the content of BlockAck when m_nMaxInflight is greater than 1
            break;
        }
        CheckBlockAck(psdu, txVector, linkId);
        m_blockAckCount++;
        if (m_blockAckCount == 2)
        {
            // corrupt the second BlockAck frame to simulate a missed BlockAck
            m_uidList.push_front(psdu->GetPacket()->GetUid());
            NS_LOG_INFO("CORRUPTED");
            m_errorModels.at(psdu->GetAddr1())->SetList(m_uidList);
        }
        break;
    }
    default:;
    }

    MultiLinkOperationsTestBase::Transmit(linkId, context, psduMap, txVector, txPowerW);
}

void
MultiLinkTxTest::CheckBlockAck(Ptr<const WifiPsdu> psdu,
                               const WifiTxVector& txVector,
                               uint8_t linkId)
{
    NS_TEST_ASSERT_MSG_EQ(m_baEnabled, true, "No BlockAck expected without BA agreement");
    NS_TEST_ASSERT_MSG_EQ((m_trafficPattern != WifiTrafficPattern::AP_TO_BCAST),
                          true,
                          "No BlockAck expected in AP to broadcast traffic pattern");

    /*
     *         ┌───────┬───────X        ┌───────┐
     *  link 0 │   0   │   1   │        │   1   │
     *  ───────┴───────┴───────┴┬──┬────┴───────┴┬───┬────────────────────────
     *                          │BA│             │ACK│
     *                          └──┘             └───┘
     *                      ┌───────┬───────┐       ┌───────┬───────┐
     *  link 1              │   2   │   3   │       │   2   │   3   │
     *  ────────────────────┴───────┴───────┴┬──X───┴───────┴───────┴┬──┬─────
     *                                       │BA│                    │BA│
     *                                       └──┘                    └──┘
     */
    auto mpdu = *psdu->begin();
    CtrlBAckResponseHeader blockAck;
    mpdu->GetPacket()->PeekHeader(blockAck);
    bool isMpdu1corrupted = (m_trafficPattern == WifiTrafficPattern::STA_TO_AP ||
                             m_trafficPattern == WifiTrafficPattern::AP_TO_STA);

    switch (m_blockAckCount)
    {
    case 0: // first BlockAck frame (all traffic patterns)
        NS_TEST_EXPECT_MSG_EQ(blockAck.IsPacketReceived(0),
                              true,
                              "MPDU 0 expected to be successfully received");
        NS_TEST_EXPECT_MSG_EQ(
            blockAck.IsPacketReceived(1),
            !isMpdu1corrupted,
            "MPDU 1 expected to be received only in STA_TO_STA/STA_TO_BCAST scenarios");
        // if there are at least two links setup, we expect all MPDUs to be inflight
        // (on distinct links)
        if (m_staMacs[0]->GetSetupLinkIds().size() > 1)
        {
            auto queue = m_sourceMac->GetTxopQueue(AC_BE);
            auto rcvMac = m_sourceMac == m_staMacs[0] ? StaticCast<WifiMac>(m_apMac)
                                                      : StaticCast<WifiMac>(m_staMacs[1]);
            auto item = queue->PeekByTidAndAddress(0, rcvMac->GetAddress());
            std::size_t nQueuedPkt = 0;
            auto delay = WifiPhy::CalculateTxDuration(psdu,
                                                      txVector,
                                                      rcvMac->GetWifiPhy(linkId)->GetPhyBand()) +
                         MicroSeconds(1); // to account for propagation delay

            while (item)
            {
                auto seqNo = item->GetHeader().GetSequenceNumber();
                NS_TEST_EXPECT_MSG_EQ(item->IsInFlight(),
                                      true,
                                      "MPDU with seqNo=" << seqNo << " is not in flight");
                auto linkIds = item->GetInFlightLinkIds();
                NS_TEST_EXPECT_MSG_EQ(linkIds.size(),
                                      1,
                                      "MPDU with seqNo=" << seqNo
                                                         << " is in flight on multiple links");
                // The first two MPDUs are in flight on the same link on which the BlockAck
                // is sent. The other two MPDUs (only for AP to STA/STA to AP scenarios) are
                // in flight on a different link.
                auto srcLinkId = m_sourceMac->GetLinkIdByAddress(mpdu->GetHeader().GetAddr1());
                NS_TEST_ASSERT_MSG_EQ(srcLinkId.has_value(),
                                      true,
                                      "Addr1 of BlockAck is not an originator's link address");
                NS_TEST_EXPECT_MSG_EQ((*linkIds.begin() == *srcLinkId),
                                      (seqNo <= 1),
                                      "MPDU with seqNo=" << seqNo
                                                         << " in flight on unexpected link");
                // check the Retry subfield and whether this MPDU is still queued
                // after the originator has processed this BlockAck

                // MPDUs acknowledged via this BlockAck are no longer queued
                bool isQueued = (seqNo > (isMpdu1corrupted ? 0 : 1));
                // The Retry subfield is set if the MPDU has not been acknowledged (i.e., it
                // is still queued) and has been transmitted on the same link as the BlockAck
                // (i.e., its sequence number is less than or equal to 1)
                bool isRetry = isQueued && seqNo <= 1;

                Simulator::Schedule(delay, [this, item, isQueued, isRetry]() {
                    NS_TEST_EXPECT_MSG_EQ(item->IsQueued(),
                                          isQueued,
                                          "MPDU with seqNo="
                                              << item->GetHeader().GetSequenceNumber() << " should "
                                              << (isQueued ? "" : "not") << " be queued");
                    NS_TEST_EXPECT_MSG_EQ(
                        item->GetHeader().IsRetry(),
                        isRetry,
                        "Unexpected value for the Retry subfield of the MPDU with seqNo="
                            << item->GetHeader().GetSequenceNumber());
                });

                nQueuedPkt++;
                item = queue->PeekByTidAndAddress(0, rcvMac->GetAddress(), item);
            }
            // Each MPDU contains an A-MSDU consisting of two MSDUs
            NS_TEST_EXPECT_MSG_EQ(nQueuedPkt, m_nPackets / 2, "Unexpected number of queued MPDUs");
        }
        break;
    case 1: // second BlockAck frame (STA to AP and AP to STA traffic patterns only)
    case 2: // third BlockAck frame (STA to AP and AP to STA traffic patterns only)
        NS_TEST_EXPECT_MSG_EQ((m_trafficPattern == WifiTrafficPattern::AP_TO_STA ||
                               m_trafficPattern == WifiTrafficPattern::STA_TO_AP),
                              true,
                              "Did not expect to receive a second BlockAck");
        // the second BlockAck is corrupted, but the data frames have been received successfully
        std::pair<uint16_t, uint16_t> seqNos;
        // if multiple links were setup, the transmission of the second A-MPDU started before
        // the end of the first one, so the second A-MPDU includes MPDUs with sequence numbers
        // 2 and 3. Otherwise, MPDU with sequence number 1 is retransmitted along with the MPDU
        // with sequence number 2.
        if (m_staMacs[0]->GetSetupLinkIds().size() > 1)
        {
            seqNos = {2, 3};
        }
        else
        {
            seqNos = {1, 2};
        }
        NS_TEST_EXPECT_MSG_EQ(blockAck.IsPacketReceived(seqNos.first),
                              true,
                              "MPDU " << seqNos.first << " expected to be successfully received");
        NS_TEST_EXPECT_MSG_EQ(blockAck.IsPacketReceived(seqNos.second),
                              true,
                              "MPDU " << seqNos.second << " expected to be successfully received");
        break;
    }
}

void
MultiLinkTxTest::L7Receive(uint8_t nodeId, Ptr<const Packet> p, const Address& addr)
{
    NS_LOG_INFO("Packet received by NODE " << +nodeId << "\n");
    m_rxPkts[nodeId]++;
}

void
MultiLinkTxTest::DoSetup()
{
    MultiLinkOperationsTestBase::DoSetup();

    if (m_baEnabled)
    {
        // Enable A-MSDU aggregation. Max A-MSDU size is set such that two MSDUs can be aggregated
        for (auto mac : std::initializer_list<Ptr<WifiMac>>{m_apMac, m_staMacs[0], m_staMacs[1]})
        {
            mac->SetAttribute("BE_MaxAmsduSize", UintegerValue(2100));
            // TODO
            mac->GetQosTxop(AC_BE)->SetAttribute("UseExplicitBarAfterMissedBlockAck",
                                                 BooleanValue(false));
            mac->GetQosTxop(AC_BE)->SetAttribute("NMaxInflights", UintegerValue(m_nMaxInflight));
        }
    }

    // install post reception error model on all devices
    for (std::size_t linkId = 0; linkId < m_apMac->GetNLinks(); linkId++)
    {
        auto errorModel = CreateObject<ListErrorModel>();
        m_errorModels[m_apMac->GetFrameExchangeManager(linkId)->GetAddress()] = errorModel;
        m_apMac->GetWifiPhy(linkId)->SetPostReceptionErrorModel(errorModel);
    }
    for (std::size_t linkId = 0; linkId < m_staMacs[0]->GetNLinks(); linkId++)
    {
        auto errorModel = CreateObject<ListErrorModel>();
        m_errorModels[m_staMacs[0]->GetFrameExchangeManager(linkId)->GetAddress()] = errorModel;
        m_staMacs[0]->GetWifiPhy(linkId)->SetPostReceptionErrorModel(errorModel);

        errorModel = CreateObject<ListErrorModel>();
        m_errorModels[m_staMacs[1]->GetFrameExchangeManager(linkId)->GetAddress()] = errorModel;
        m_staMacs[1]->GetWifiPhy(linkId)->SetPostReceptionErrorModel(errorModel);
    }
}

void
MultiLinkTxTest::StartTraffic()
{
    const Time duration = Seconds(1);
    Address destAddr;

    switch (m_trafficPattern)
    {
    case WifiTrafficPattern::STA_TO_STA:
        m_sourceMac = m_staMacs[0];
        destAddr = m_staMacs[1]->GetDevice()->GetAddress();
        break;
    case WifiTrafficPattern::STA_TO_AP:
        m_sourceMac = m_staMacs[0];
        destAddr = m_apMac->GetDevice()->GetAddress();
        break;
    case WifiTrafficPattern::AP_TO_STA:
        m_sourceMac = m_apMac;
        destAddr = m_staMacs[1]->GetDevice()->GetAddress();
        break;
    case WifiTrafficPattern::AP_TO_BCAST:
        m_sourceMac = m_apMac;
        destAddr = Mac48Address::GetBroadcast();
        break;
    case WifiTrafficPattern::STA_TO_BCAST:
        m_sourceMac = m_staMacs[0];
        destAddr = Mac48Address::GetBroadcast();
        break;
    }

    PacketSocketHelper packetSocket;
    packetSocket.Install(m_apMac->GetDevice()->GetNode());
    packetSocket.Install(m_staMacs[0]->GetDevice()->GetNode());
    packetSocket.Install(m_staMacs[1]->GetDevice()->GetNode());

    PacketSocketAddress socket;
    socket.SetSingleDevice(m_sourceMac->GetDevice()->GetIfIndex());
    socket.SetPhysicalAddress(destAddr);
    socket.SetProtocol(1);

    // install first client application generating at most 4 packets
    auto client1 = CreateObject<PacketSocketClient>();
    client1->SetAttribute("PacketSize", UintegerValue(1000));
    client1->SetAttribute("MaxPackets", UintegerValue(std::min<std::size_t>(m_nPackets, 4)));
    client1->SetAttribute("Interval", TimeValue(MicroSeconds(0)));
    client1->SetRemote(socket);
    m_sourceMac->GetDevice()->GetNode()->AddApplication(client1);
    client1->SetStartTime(Seconds(0)); // now
    client1->SetStopTime(duration);

    if (m_nPackets > 4)
    {
        // install a second client application generating the remaining packets
        auto client2 = CreateObject<PacketSocketClient>();
        client2->SetAttribute("PacketSize", UintegerValue(1000));
        client2->SetAttribute("MaxPackets", UintegerValue(m_nPackets - 4));
        client2->SetAttribute("Interval", TimeValue(MicroSeconds(0)));
        client2->SetRemote(socket);
        m_sourceMac->GetDevice()->GetNode()->AddApplication(client2);
        // start during transmission of first A-MPDU, if multiple links are setup
        client2->SetStartTime(MilliSeconds(3));
        client2->SetStopTime(duration);
    }

    // install a server on all nodes
    for (auto nodeIt = NodeList::Begin(); nodeIt != NodeList::End(); nodeIt++)
    {
        Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
        server->SetLocal(socket);
        (*nodeIt)->AddApplication(server);
        server->SetStartTime(Seconds(0)); // now
        server->SetStopTime(duration);
    }

    for (std::size_t nodeId = 0; nodeId < NodeList::GetNNodes(); nodeId++)
    {
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(nodeId) +
                                          "/ApplicationList/*/$ns3::PacketSocketServer/Rx",
                                      MakeCallback(&MultiLinkTxTest::L7Receive, this).Bind(nodeId));
    }

    Simulator::Stop(duration);
}

void
MultiLinkTxTest::DoRun()
{
    Simulator::Run();

    // Expected number of packets received by each node (AP, STA 0, STA 1) at application layer
    std::array<std::size_t, 3> expectedRxPkts{};

    switch (m_trafficPattern)
    {
    case WifiTrafficPattern::STA_TO_STA:
    case WifiTrafficPattern::AP_TO_STA:
        // only STA 1 receives the m_nPackets packets that have been transmitted
        expectedRxPkts[2] = m_nPackets;
        break;
    case WifiTrafficPattern::STA_TO_AP:
        // only the AP receives the m_nPackets packets that have been transmitted
        expectedRxPkts[0] = m_nPackets;
        break;
    case WifiTrafficPattern::AP_TO_BCAST:
        // the AP replicates the broadcast frames on all the links, hence each station
        // receives the m_nPackets packets N times, where N is the number of setup link
        expectedRxPkts[1] = m_nPackets * m_staMacs[0]->GetSetupLinkIds().size();
        expectedRxPkts[2] = m_nPackets * m_staMacs[1]->GetSetupLinkIds().size();
        break;
    case WifiTrafficPattern::STA_TO_BCAST:
        // the AP receives the m_nPackets packets and then replicates them on all the links,
        // hence STA 1 receives m_nPackets packets N times, where N is the number of setup link
        expectedRxPkts[0] = m_nPackets;
        expectedRxPkts[2] = m_nPackets * m_staMacs[1]->GetSetupLinkIds().size();
        break;
    }

    NS_TEST_EXPECT_MSG_EQ(+m_rxPkts[0],
                          +expectedRxPkts[0],
                          "Unexpected number of packets received by the AP");
    NS_TEST_EXPECT_MSG_EQ(+m_rxPkts[1],
                          +expectedRxPkts[1],
                          "Unexpected number of packets received by STA 0");
    NS_TEST_EXPECT_MSG_EQ(+m_rxPkts[2],
                          +expectedRxPkts[2],
                          "Unexpected number of packets received by STA 1");

    // check that the expected number of BlockAck frames are transmitted
    if (m_baEnabled && m_nMaxInflight == 1)
    {
        std::size_t expectedBaCount = 0;

        switch (m_trafficPattern)
        {
        case WifiTrafficPattern::STA_TO_AP:
        case WifiTrafficPattern::AP_TO_STA:
            // two A-MPDUs are transmitted and one BlockAck is corrupted
            expectedBaCount = 3;
            break;
        case WifiTrafficPattern::STA_TO_STA:
        case WifiTrafficPattern::STA_TO_BCAST:
            // only one A-MPDU is transmitted and the BlockAck is not corrupted
            expectedBaCount = 1;
            break;
        default:;
        }
        NS_TEST_EXPECT_MSG_EQ(m_blockAckCount,
                              expectedBaCount,
                              "Unexpected number of BlockAck frames");
    }

    // check that setting the QosTxop::NMaxInflights attribute has the expected effect.
    // We do not support sending an MPDU multiple times concurrently without Block Ack
    // agreement. Also, broadcast frames are already duplicated and sent on all links.
    if (m_baEnabled && m_trafficPattern != WifiTrafficPattern::AP_TO_BCAST)
    {
        NS_TEST_EXPECT_MSG_EQ(
            m_inflightCount.size(),
            m_nPackets / 2,
            "Did not collect number of simultaneous transmissions for all data frames");

        auto nMaxInflight = std::min(m_nMaxInflight, m_staMacs[0]->GetSetupLinkIds().size());
        std::size_t maxCount = 0;
        for (const auto& [seqNo, count] : m_inflightCount)
        {
            NS_TEST_EXPECT_MSG_LT_OR_EQ(
                count,
                nMaxInflight,
                "MPDU with seqNo=" << seqNo
                                   << " transmitted simultaneously more times than allowed");
            maxCount = std::max(maxCount, count);
        }

        NS_TEST_EXPECT_MSG_EQ(
            maxCount,
            nMaxInflight,
            "Expected that at least one data frame was transmitted simultaneously a number of "
            "times equal to the NMaxInflights attribute");
    }

    Simulator::Destroy();
}

/**
 * \ingroup wifi-test
 * \ingroup tests
 *
 * \brief wifi 11be MLD Test Suite
 */
class WifiMultiLinkOperationsTestSuite : public TestSuite
{
  public:
    WifiMultiLinkOperationsTestSuite();
};

WifiMultiLinkOperationsTestSuite::WifiMultiLinkOperationsTestSuite()
    : TestSuite("wifi-mlo", UNIT)
{
    using ParamsTuple = std::tuple<std::vector<std::string>,
                                   std::vector<std::string>,
                                   std::vector<std::pair<uint8_t, uint8_t>>,
                                   std::vector<uint8_t>>;

    AddTestCase(new GetRnrLinkInfoTest(), TestCase::QUICK);

    for (const auto& [staChannels, apChannels, setupLinks, fixedPhyBands] :
         {// matching channels: setup all links
          ParamsTuple({"{36, 0, BAND_5GHZ, 0}", "{2, 0, BAND_2_4GHZ, 0}", "{1, 0, BAND_6GHZ, 0}"},
                      {"{36, 0, BAND_5GHZ, 0}", "{2, 0, BAND_2_4GHZ, 0}", "{1, 0, BAND_6GHZ, 0}"},
                      {{0, 0}, {1, 1}, {2, 2}},
                      {}),
          // non-matching channels, matching PHY bands: setup all links
          ParamsTuple({"{108, 0, BAND_5GHZ, 0}", "{36, 0, BAND_5GHZ, 0}", "{1, 0, BAND_6GHZ, 0}"},
                      {"{36, 0, BAND_5GHZ, 0}", "{120, 0, BAND_5GHZ, 0}", "{5, 0, BAND_6GHZ, 0}"},
                      {{1, 0}, {0, 1}, {2, 2}},
                      {}),
          // non-AP MLD switches band on some links to setup 3 links
          ParamsTuple({"{2, 0, BAND_2_4GHZ, 0}", "{1, 0, BAND_6GHZ, 0}", "{36, 0, BAND_5GHZ, 0}"},
                      {"{36, 0, BAND_5GHZ, 0}", "{9, 0, BAND_6GHZ, 0}", "{120, 0, BAND_5GHZ, 0}"},
                      {{2, 0}, {0, 1}, {1, 2}},
                      {}),
          // the first link of the non-AP MLD cannot change PHY band and no AP is operating on
          // that band, hence only 2 links are setup
          ParamsTuple(
              {"{2, 0, BAND_2_4GHZ, 0}", "{36, 0, BAND_5GHZ, 0}", "{8, 20, BAND_2_4GHZ, 0}"},
              {"{36, 0, BAND_5GHZ, 0}", "{1, 0, BAND_6GHZ, 0}", "{120, 0, BAND_5GHZ, 0}"},
              {{1, 0}, {2, 1}},
              {0}),
          // the first link of the non-AP MLD cannot change PHY band and no AP is operating on
          // that band; the second link of the non-AP MLD cannot change PHY band and there is
          // an AP operating on the same channel; hence 2 links are setup
          ParamsTuple(
              {"{2, 0, BAND_2_4GHZ, 0}", "{36, 0, BAND_5GHZ, 0}", "{8, 20, BAND_2_4GHZ, 0}"},
              {"{36, 0, BAND_5GHZ, 0}", "{1, 0, BAND_6GHZ, 0}", "{120, 0, BAND_5GHZ, 0}"},
              {{1, 0}, {2, 1}},
              {0, 1}),
          // the first link of the non-AP MLD cannot change PHY band and no AP is operating on
          // that band; the second link of the non-AP MLD cannot change PHY band and there is
          // an AP operating on the same channel; the third link of the non-AP MLD cannot
          // change PHY band and there is an AP operating on the same band (different channel);
          // hence 2 links are setup by switching channel (not band) on the third link
          ParamsTuple({"{2, 0, BAND_2_4GHZ, 0}", "{36, 0, BAND_5GHZ, 0}", "{60, 0, BAND_5GHZ, 0}"},
                      {"{36, 0, BAND_5GHZ, 0}", "{1, 0, BAND_6GHZ, 0}", "{120, 0, BAND_5GHZ, 0}"},
                      {{1, 0}, {2, 2}},
                      {0, 1, 2}),
          // the first link of the non-AP MLD cannot change PHY band and no AP is operating on
          // that band; the second link of the non-AP MLD cannot change PHY band and there is
          // an AP operating on the same channel; hence one link only is setup
          ParamsTuple({"{2, 0, BAND_2_4GHZ, 0}", "{36, 0, BAND_5GHZ, 0}"},
                      {"{36, 0, BAND_5GHZ, 0}", "{1, 0, BAND_6GHZ, 0}", "{120, 0, BAND_5GHZ, 0}"},
                      {{1, 0}},
                      {0, 1}),
          // non-AP MLD has only two STAs and setups two links
          ParamsTuple({"{2, 0, BAND_2_4GHZ, 0}", "{36, 0, BAND_5GHZ, 0}"},
                      {"{36, 0, BAND_5GHZ, 0}", "{1, 0, BAND_6GHZ, 0}", "{120, 0, BAND_5GHZ, 0}"},
                      {{0, 1}, {1, 0}},
                      {}),
          // single link non-AP STA associates with an AP affiliated with an AP MLD
          ParamsTuple({"{120, 0, BAND_5GHZ, 0}"},
                      {"{36, 0, BAND_5GHZ, 0}", "{1, 0, BAND_6GHZ, 0}", "{120, 0, BAND_5GHZ, 0}"},
                      {{0, 2}},
                      {}),
          // a STA affiliated with a non-AP MLD associates with a single link AP
          ParamsTuple({"{36, 0, BAND_5GHZ, 0}", "{1, 0, BAND_6GHZ, 0}", "{120, 0, BAND_5GHZ, 0}"},
                      {"{120, 0, BAND_5GHZ, 0}"},
                      {{2, 0}},
                      {})})
    {
        AddTestCase(new MultiLinkSetupTest(staChannels, apChannels, setupLinks, fixedPhyBands),
                    TestCase::QUICK);

        for (const auto& trafficPattern : {WifiTrafficPattern::STA_TO_STA,
                                           WifiTrafficPattern::STA_TO_AP,
                                           WifiTrafficPattern::AP_TO_STA,
                                           WifiTrafficPattern::AP_TO_BCAST,
                                           WifiTrafficPattern::STA_TO_BCAST})
        {
            // No Block Ack agreement
            AddTestCase(new MultiLinkTxTest(trafficPattern,
                                            WifiBaEnabled::NO,
                                            1,
                                            staChannels,
                                            apChannels,
                                            fixedPhyBands),
                        TestCase::QUICK);
            // Block Ack agreement with nMaxInflight=1
            AddTestCase(new MultiLinkTxTest(trafficPattern,
                                            WifiBaEnabled::YES,
                                            1,
                                            staChannels,
                                            apChannels,
                                            fixedPhyBands),
                        TestCase::QUICK);
            // Block Ack agreement with nMaxInflight=2
            AddTestCase(new MultiLinkTxTest(trafficPattern,
                                            WifiBaEnabled::YES,
                                            2,
                                            staChannels,
                                            apChannels,
                                            fixedPhyBands),
                        TestCase::QUICK);
        }
    }
}

static WifiMultiLinkOperationsTestSuite g_wifiMultiLinkOperationsTestSuite; ///< the test suite
