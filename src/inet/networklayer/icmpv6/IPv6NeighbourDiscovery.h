/**
 * Copyright (C) 2005 Andras Varga
 * Copyright (C) 2005 Wei Yang, Ng
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __INET_IPV6NEIGHBOURDISCOVERY_H
#define __INET_IPV6NEIGHBOURDISCOVERY_H

#include <vector>
#include <set>
#include <map>

#include "inet/common/packet/Packet.h"
#include "inet/networklayer/contract/ipv6/IPv6Address.h"
#include "inet/networklayer/icmpv6/IPv6NDMessage_m.h"
#include "inet/networklayer/icmpv6/IPv6NeighbourCache.h"
#include "inet/common/lifecycle/ILifecycle.h"

namespace inet {

//Forward declarations:
class Icmpv6;
class IInterfaceTable;
class InterfaceEntry;
class Ipv6Header;
class Ipv6RoutingTable;

#ifdef WITH_xMIPv6
class xMIPv6;
#endif /* WITH_xMIPv6 */

/**
 * Implements RFC 2461 Neighbor Discovery for IPv6.
 */
class INET_API Ipv6NeighbourDiscovery : public cSimpleModule, public ILifecycle
{
  public:
    typedef std::vector<Packet *> MsgPtrVector;
    typedef Ipv6NeighbourCache::Key Key;    //for convenience
    typedef Ipv6NeighbourCache::Neighbour Neighbour;    // for convenience
    typedef Ipv6NeighbourCache::DefaultRouterList DefaultRouterList;    // for convenience

  public:
    Ipv6NeighbourDiscovery();
    virtual ~Ipv6NeighbourDiscovery();

  private:
    static simsignal_t startDADSignal;

  public:
    /**
     * Public method, to be invoked from the IPv6 module to determine
     * link-layer address and the output interface of the next hop.
     *
     * If the neighbor cache does not contain this address or it's in the
     * state INCOMPLETE, this method will return the nullptr address, and the
     * IPv6 module should then send the datagram here to IPv6NeighbourDiscovery
     * where it will be stored until neighbour resolution completes.
     *
     * If the neighbour cache entry is STALE (or REACHABLE but more than
     * reachableTime elapsed since reachability was last confirmed),
     * the link-layer address is still returned and IPv6 can send the
     * datagram, but simultaneously, this call should trigger the Neighbour
     * Unreachability Detection procedure to start in the
     * IPv6NeighbourDiscovery module.
     */
    const MacAddress& resolveNeighbour(const Ipv6Address& nextHop, int interfaceId);

    /**
     * Public method, it can be invoked from the IPv6 module or any other
     * module to let Neighbour Discovery know that the reachability
     * of the given neighbor has just been confirmed (e.g. TCP received
     * ACK of new data from it). Neighbour Discovery can then update
     * the neighbour cache with this information, and cancel the
     * Neighbour Unreachability Detection procedure if it is currently
     * running.
     */
    virtual void reachabilityConfirmed(const Ipv6Address& neighbour, int interfaceId);

  protected:

    //Packets awaiting Address Resolution or Next-Hop Determination.
    cQueue pendingQueue;

    IInterfaceTable *ift = nullptr;
    Ipv6RoutingTable *rt6 = nullptr;
    Icmpv6 *icmpv6 = nullptr;

#ifdef WITH_xMIPv6
    xMIPv6 *mipv6 = nullptr;    // in case the node has MIP support
#endif /* WITH_xMIPv6 */

    Ipv6NeighbourCache neighbourCache;
    typedef std::set<cMessage *> RaTimerList;    //FIXME add comparator for stable fingerprints!

    // stores information about a pending Duplicate Address Detection for
    // an interface
    struct DadEntry
    {
        int interfaceId;    // interface on which DAD is performed
        Ipv6Address address;    // link-local address subject to DAD
        int numNSSent;    // number of DAD solicitations sent since start of sim
        cMessage *timeoutMsg;    // the message to cancel when NA is received
    };
    typedef std::set<DadEntry *> DadList;    //FIXME why ptrs are stored?    //FIXME add comparator for stable fingerprints!

    //stores information about Router Discovery for an interface
    struct RdEntry
    {
        int interfaceId;    //interface on which Router Discovery is performed
        unsigned int numRSSent;    //number of Router Solicitations sent since start of sim
        cMessage *timeoutMsg;    //the message to cancel when RA is received
    };
    typedef std::set<RdEntry *> RdList;    //FIXME why ptrs are stored?    //FIXME add comparator for stable fingerprints!

    //An entry that stores information for an Advertising Interface
    struct AdvIfEntry
    {
        int interfaceId;
        unsigned int numRASent;    //number of Router Advertisements sent since start of sim
        simtime_t nextScheduledRATime;    //stores time when next RA will be sent.
        cMessage *raTimeoutMsg;    //the message to cancel when resetting RA timer
    };
    typedef std::set<AdvIfEntry *> AdvIfList;    //FIXME why ptrs are stored?    //FIXME add comparator for stable fingerprints!

    //List of periodic RA msgs(used only for router interfaces)
    RaTimerList raTimerList;

    //List of pending Duplicate Address Detections
    DadList dadList;

    //List of pending Router & Prefix Discoveries
    RdList rdList;

    //List of Advertising Interfaces
    AdvIfList advIfList;

#ifdef WITH_xMIPv6
    // An entry that stores information for configuring the global unicast
    // address, after DAD was succesfully performed
    struct DadGlobalEntry
    {
        bool hFlag;    // home network flag from RA
        simtime_t validLifetime;    // valid lifetime of the received prefix
        simtime_t preferredLifetime;    // preferred lifetime of the received prefix
        Ipv6Address addr;    // the address with scope > link local that the interface will get

        //bool returnedHome; // MIPv6-related: whether we returned home after a visit in a foreign network
        Ipv6Address CoA;    // MIPv6-related: the old CoA, in case we returned home
    };
    typedef std::map<InterfaceEntry *, DadGlobalEntry> DadGlobalList;    //FIXME add comparator for stable fingerprints!
    DadGlobalList dadGlobalList;
#endif /* WITH_xMIPv6 */

  protected:
    /************************Miscellaneous Stuff***************************/
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void processNDMessage(Packet *packet, const Icmpv6Header *msg);
    virtual bool handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback) override;
    virtual void finish() override;

    virtual void processIPv6Datagram(Packet *packet);
    virtual Ipv6NeighbourDiscovery::AdvIfEntry *fetchAdvIfEntry(InterfaceEntry *ie);
    virtual Ipv6NeighbourDiscovery::RdEntry *fetchRDEntry(InterfaceEntry *ie);
    /************************End of Miscellaneous Stuff********************/

    /**
     *  This function accepts the datagram's destination address and attempts
     *  to determine the destination's next hop address and interface ID by:
     *  (1) looking up the destination cache, (2)looking up the routing table,
     *  or (3) selecting a default router. It then updates the destination
     *  cache. If no default router can be selected than we assume the
     *  destination address to be onlink and simply return any available
     *  interface.
     */
    virtual Ipv6Address determineNextHop(const Ipv6Address& destAddr, int& outIfID);
    virtual void initiateNeighbourUnreachabilityDetection(Neighbour *neighbour);
    virtual void processNUDTimeout(cMessage *timeoutMsg);
    virtual Ipv6Address selectDefaultRouter(int& outIfID);
    /**
     *  RFC 2461: Section 6.3.5
     *  Whenever the invalidation timer expires for a Prefix List entry, that
     *  entry is discarded. No existing Destination Cache entries need be
     *  updated, however. Should a reachability problem arise with an
     *  existing Neighbor Cache entry, Neighbor Unreachability Detection will
     *  perform any needed recovery.
     */
    virtual void timeoutPrefixEntry(const Ipv6Address& destPrefix, int prefixLength);
    /**
     *  RFC 2461: Section 6.3.5
     *  Whenever the Lifetime of an entry in the Default Router List expires,
     *  that entry is discarded. When removing a router from the Default
     *  Router list, the node MUST update the Destination Cache in such a way
     *  that all entries using the router perform next-hop determination
     *  again rather than continue sending traffic to the (deleted) router.
     */
    virtual void timeoutDefaultRouter(const Ipv6Address& addr, int interfaceID);
    /**
     *  This method attempts to resolve the given neighbour's link-layer address.
     *  The source address of the packet prompting address resolution is also
     *  given in order to decide the source address of the NS to be sent.
     *  nceKey stores 2 pieces of information (Neighbour address and Interface ID)
     *  which is needed for addr resolution and access to the corresponding
     *  nce.
     */
    virtual void initiateAddressResolution(const Ipv6Address& dgSrcAddr,
            Neighbour *nce);
    /**
     *  Resends a NS packet to the address intended for address resolution.
     *  TODO: Not implemented yet!
     */
    virtual void processARTimeout(cMessage *arTimeoutMsg);
    /**
     *  Drops specific queued packets for a specific NCE AR-timeout.
     *  TODO: Not implemented yet!
     */
    virtual void dropQueuedPacketsAwaitingAR(Neighbour *nce);
    /**
     *  Create control info and assigns it to a msg. Returns a copy of the
     *  msg with the control info.
     */
    virtual void sendPacketToIPv6Module(cMessage *msg, const Ipv6Address& destAddr,
            const Ipv6Address& srcAddr, int interfaceId);

    /**
     *  Send off any queued packets within the Neighbour Discovery module
     *  awaiting address resolution.
     */
    virtual void sendQueuedPacketsToIPv6Module(Neighbour *nce);

    /**
     *  Initiating DAD means to send off a Neighbour Solicitation with its
     *  target address set as this node's tentative link-local address.
     */
    virtual void initiateDAD(const Ipv6Address& tentativeAddr, InterfaceEntry *ie);

    /**
     *  Sends a scheduled DAD NS packet. If number of sends is equals or more
     *  than dupAddrDetectTransmits, then permantly assign target link local
     *  address as permanent address for given interface entry.
     */
    virtual void processDADTimeout(cMessage *msg);

    /**
     * Permanently assign the given address for the given interface entry.
     * To be called after successful DAD.
     */
    virtual void makeTentativeAddressPermanent(const Ipv6Address& tentativeAddr, InterfaceEntry *ie);

    /************Address Autoconfiguration Stuff***************************/
    /**
     *  as it is not possbile to explicitly define RFC 2462. ND is the next
     *  best place to do this.
     *
     *  RFC 2462-IPv6 Stateless Address Autoconfiguration: Section 1
     *  The autoconfiguration process specified in this document applies only
     *  to hosts and not routers. Since host autoconfiguration uses
     *  information advertised by routers, routers will need to be configured
     *  by some other means. However, it is expected that routers will
     *  generate link-local addresses using the mechanism described in this
     *  document. In addition, routers are expected to successfully pass the
     *  Duplicate Address Detection procedure described in this document on
     *  all addresses prior to assigning them to an interface.
     */
    virtual void assignLinkLocalAddress(cMessage *timerMsg);

    /************End Of Address Autoconfiguration Stuff********************/

    /************Router Solicitation Stuff*********************************/
    virtual void createAndSendRSPacket(InterfaceEntry *ie);
    virtual void initiateRouterDiscovery(cMessage *msg);
    /**
     *  RFC 2461: Section 6.3.7 4th paragraph
     *  Once the host sends a Router Solicitation, and receives a valid
     *  Router Advertisement with a non-zero Router Lifetime, the host MUST
     *  desist from sending additional solicitations on that interface,
     *
     *  Cancel Router Discovery on the Interface where a RA was received with
     *  the given Interface Entry.
     */
    virtual void cancelRouterDiscovery(InterfaceEntry *ie);
    virtual void processRDTimeout(cMessage *msg);
    virtual void processRSPacket(Packet *packet, const Ipv6RouterSolicitation *rs);
    virtual bool validateRSPacket(Packet *packet, const Ipv6RouterSolicitation *rs);
    /************End of Router Solicitation Stuff**************************/

    /************Router Advertisment Stuff*********************************/
    virtual void createAndSendRAPacket(const Ipv6Address& destAddr,
            InterfaceEntry *ie);
    virtual void processRAPacket(Packet *packet, const Ipv6RouterAdvertisement *ra);
    virtual void processRAForRouterUpdates(Packet *packet, const Ipv6RouterAdvertisement *ra);
    //RFC 2461: Section 6.3.4
    /*Note: Implementations can choose to process the on-link aspects of the
       prefixes separately from the address autoconfiguration aspects of the
       prefixes by, e.g., passing a copy of each valid Router Advertisement message
       to both an "on-link" and an "addrconf" function. Each function can then
       operate independently on the prefixes that have the appropriate flag set.*/
    virtual void processRAPrefixInfo(const Ipv6RouterAdvertisement *ra, InterfaceEntry *ie);

#ifndef WITH_xMIPv6
    virtual void processRAPrefixInfoForAddrAutoConf(const Ipv6NdPrefixInformation& prefixInfo,
            InterfaceEntry *ie);
#else /* WITH_xMIPv6 */
    virtual void processRAPrefixInfoForAddrAutoConf(const Ipv6NdPrefixInformation& prefixInfo,
            InterfaceEntry *ie, bool hFlag = false);    // overloaded method - 3.9.07 CB
#endif /* WITH_xMIPv6 */

    /**
     *  Create a timer for the given interface entry that sends periodic
     *  Router Advertisements
     */
    virtual void createRATimer(InterfaceEntry *ie);

    /**
     *  Reset the given interface entry's Router Advertisement timer. This is
     *  usually done when a router interface responds (by replying with a Router
     *  Advertisement sent to the All-Node multicast group)to a router solicitation
     *  Also see: RFC 2461, Section 6.2.6
     */
    virtual void resetRATimer(InterfaceEntry *ie);
    virtual void sendPeriodicRA(cMessage *msg);
    virtual void sendSolicitedRA(cMessage *msg);
    virtual bool validateRAPacket(Packet *packet, const Ipv6RouterAdvertisement *ra);
    /************End of Router Advertisement Stuff*************************/

    /************Neighbour Solicitaton Stuff*******************************/
    virtual void createAndSendNSPacket(const Ipv6Address& nsTargetAddr, const Ipv6Address& dgDestAddr,
            const Ipv6Address& dgSrcAddr, InterfaceEntry *ie);
    virtual void processNSPacket(Packet *packet, const Ipv6NeighbourSolicitation *ns);
    virtual bool validateNSPacket(Packet *packet, const Ipv6NeighbourSolicitation *ns);
    virtual void processNSForTentativeAddress(Packet *packet, const Ipv6NeighbourSolicitation *ns);
    virtual void processNSForNonTentativeAddress(Packet *packet, const Ipv6NeighbourSolicitation *ns, InterfaceEntry *ie);
    virtual void processNSWithSpecifiedSrcAddr(Packet *packet, const Ipv6NeighbourSolicitation *ns, InterfaceEntry *ie);
    /************End Of Neighbour Solicitation Stuff***********************/

    /************Neighbour Advertisment Stuff)*****************************/

#ifdef WITH_xMIPv6
    Ipv6NeighbourAdvertisement *createAndSendNAPacket(Ipv6NeighbourSolicitation *ns,
            const Ipv6Address& nsSrcAddr, const Ipv6Address& nsDestAddr, InterfaceEntry *ie);
#endif /* WITH_xMIPv6 */

    virtual void sendSolicitedNA(Packet *packet, const Ipv6NeighbourSolicitation *ns, InterfaceEntry *ie);

#ifdef WITH_xMIPv6

  public:    // update 12.9.07 - CB
#endif /* WITH_xMIPv6 */
    virtual void sendUnsolicitedNA(InterfaceEntry *ie);

#ifdef WITH_xMIPv6

  protected:    // update 12.9.07 - CB
#endif /* WITH_xMIPv6 */

    virtual void processNAPacket(Packet *packet, const Ipv6NeighbourAdvertisement *na);
    virtual bool validateNAPacket(Packet *packet, const Ipv6NeighbourAdvertisement *na);
    virtual void processNAForIncompleteNCEState(const Ipv6NeighbourAdvertisement *na, Ipv6NeighbourCache::Neighbour *nce);
    virtual void processNAForOtherNCEStates(const Ipv6NeighbourAdvertisement *na, Ipv6NeighbourCache::Neighbour *nce);
    /************End Of Neighbour Advertisement Stuff**********************/

    /************Redirect Message Stuff************************************/
    virtual void createAndSendRedirectPacket(InterfaceEntry *ie);
    virtual void processRedirectPacket(const Ipv6Redirect *redirect);
    /************End Of Redirect Message Stuff*****************************/

#ifdef WITH_xMIPv6
    /* Determine that this router can communicate with wireless nodes
     * on the LAN connected to the given interface.
     * The result is true if the interface is a wireless interface
     * or connected to an wireless access point.
     *
     * If wireless nodes can be present on the LAN, the router sends
     * RAs more frequently in accordance with the MIPv6 specification
     * (RFC 3775 7.5.).
     */
    virtual bool canServeWirelessNodes(InterfaceEntry *ie);
#endif /* WITH_xMIPv6 */

    /**
     *  RFC2463 Section 3.1: Destination Unreachable Message
     *  Send an unreachable message to the IPv6 module.
     *  TODO: Relocate to ICMPv6 module
     */
    /*Icmpv6DestUnreachableMsg *createAndSendUnreachableMessage(
        const IPv6Address& destAddress, InterfaceEntry *ie);*/

#ifdef WITH_xMIPv6

  public:
    void invalidateNeigbourCache();

  protected:
    void routersUnreachabilityDetection(const InterfaceEntry *ie);    // 3.9.07 - CB
    bool isWirelessInterface(const InterfaceEntry *ie);
    bool isWirelessAccessPoint(cModule *module);
#endif /* WITH_xMIPv6 */
};

} // namespace inet

#endif    //IPV6NEIGHBOURDISCOVERY_H

