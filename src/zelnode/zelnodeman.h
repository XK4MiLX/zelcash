// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2019 The Zel developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef ZELCASHNODES_ZELNODEMAN_H
#define ZELCASHNODES_ZELNODEMAN_H

#include "base58.h"
#include "key.h"
#include "main.h"
#include "zelnode/zelnode.h"
#include "net.h"
#include "sync.h"
#include "util.h"

#define ZELNODES_DUMP_SECONDS (15 * 60)
#define ZELNODES_DSEG_SECONDS (3 * 60 * 60)

using namespace std;

class ZelnodeMan;

extern ZelnodeMan zelnodeman;
void DumpZelnodes();

class ZelnodeMan
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // critical section to protect the inner data structures specifically on messaging
    mutable CCriticalSection cs_process_message;

    // map to hold all MNs
    std::map<CTxIn, Zelnode> mapCUMULUSZelnodes;
    std::map<CTxIn, Zelnode> mapNIMBUSZelnodes;
    std::map<CTxIn, Zelnode> mapSTRATUSZelnodes;
    // who's asked for the Zelnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForZelnodeList;
    // who we asked for the Zelnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForZelnodeList;
    // which Zelnodes we've asked for
    std::map<COutPoint, int64_t> mWeAskedForZelnodeListEntry;

public:
    // Keep track of all broadcasts I've seen
    map<uint256, ZelnodeBroadcast> mapSeenZelnodeBroadcast;
    // Keep track of all pings I've seen
    map<uint256, ZelnodePing> mapSeenZelnodePing;

    // keep track of dsq count to prevent zelnode from gaming obfuscation queue
    int64_t nDsqCount; // TODO will probably not need this, as obfuscation isn't needed.

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        LOCK(cs);
        READWRITE(mapCUMULUSZelnodes);
        READWRITE(mapNIMBUSZelnodes);
        READWRITE(mapSTRATUSZelnodes);
        READWRITE(mAskedUsForZelnodeList);
        READWRITE(mWeAskedForZelnodeList);
        READWRITE(mWeAskedForZelnodeListEntry);
        READWRITE(nDsqCount);

        READWRITE(mapSeenZelnodeBroadcast);
        READWRITE(mapSeenZelnodePing);
    }

    ZelnodeMan();
    ZelnodeMan(ZelnodeMan& other);

    /// Add an entry
    bool Add(Zelnode& mn);

    /// Ask (source) node for znb
    void AskForZN(CNode* pnode, CTxIn& vin);

    /// Check all Zelnodes
    void Check();

    /// Check all Zelnodes and remove inactive
    void CheckAndRemove(bool forceExpiredRemoval = false);

    /// Clear Zelnode vector
    void Clear();

    int CountEnabled(int protocolVersion = -1, int nNodeTier = 0);

    void CountNetworks(int protocolVersion, int& ipv4, int& ipv6, int& onion);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    Zelnode* Find(const CScript& payee);
    Zelnode* Find(const CTxIn& vin);
    Zelnode* Find(const CPubKey& pubKeyZelnode);

    /// Find an entry in the zelnode list that is next to be paid
    vector<Zelnode*> GetNextZelnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCUMULUSCount, int& nNIMBUSCount, int& nSTRATUSCount);

    /// Get the current winner for this block
    bool GetCurrentZelnode(Zelnode& winner, int nNodeTier, int mod = 1, int64_t nBlockHeight = 0, int minProtocol = 0);


    std::vector<Zelnode> GetFullZelnodeVector(int nZelnodeTier)
    {
        Check();
        if (nZelnodeTier == Zelnode::CUMULUS) {
            return GetFullCUMULUSZelnodeVector();
        } else if (nZelnodeTier == Zelnode::NIMBUS) {
            return GetFullNIMBUSZelnodeVector();
        } else if (nZelnodeTier == Zelnode::STRATUS) {
            return GetFullSTRATUSZelnodeVector();
        }

        return std::vector<Zelnode>();
    }

    std::vector<Zelnode> GetAllZelnodeVector()
    {
        std::vector<Zelnode> vecZelnode;

        std::vector<Zelnode> basic = GetFullCUMULUSZelnodeVector();
        std::vector<Zelnode> super = GetFullNIMBUSZelnodeVector();
        std::vector<Zelnode> bamf = GetFullSTRATUSZelnodeVector();

        vecZelnode = basic;
        vecZelnode.insert(vecZelnode.end(), super.begin(), super.end());
        vecZelnode.insert(vecZelnode.end(), bamf.begin(), bamf.end());

        return vecZelnode;
    }

    std::vector<pair<int, Zelnode> > GetZelnodeRanks(int nNodeTier, int64_t nBlockHeight, int minProtocol = 0);
    int GetZelnodeRank(const CTxIn& vin, int64_t nBlockHeight, int minProtocol = 0, bool fOnlyActive = true);
 //   Zelnode* GetZelnodeByRank(int nRank, int64_t nBlockHeight, int minProtocol = 0, bool fOnlyActive = true);

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    /// Return the number of (unique) Zelnodes
    int size() { return mapCUMULUSZelnodes.size() + mapNIMBUSZelnodes.size() + mapSTRATUSZelnodes.size(); }

    /// Return the number of Zelnode older than (default) 8000 seconds
    int stable_size ();

    std::string ToString() const;

    void Remove(CTxIn vin);

    int GetEstimatedZelnode(int nBlock);

    /// Update zelnode list and maps using provided ZelnodeBroadcast
    void UpdateZelnodeList(ZelnodeBroadcast znb);

private:
    /** These below methods must be called by the GetFullZelnodeVector function. Do not call these methods by themselves */
    std::vector<Zelnode> GetFullCUMULUSZelnodeVector()
    {
        std::vector<Zelnode> ret;
        for (const pair<CTxIn, Zelnode> entry : mapCUMULUSZelnodes)
            ret.emplace_back(entry.second);

        return ret;
    }

    std::vector<Zelnode> GetFullNIMBUSZelnodeVector()
    {
        std::vector<Zelnode> ret;
        for (const pair<CTxIn, Zelnode> entry : mapNIMBUSZelnodes)
            ret.emplace_back(entry.second);

        return ret;
    }

    std::vector<Zelnode> GetFullSTRATUSZelnodeVector()
    {
        std::vector<Zelnode> ret;
        for (const pair<CTxIn, Zelnode> entry : mapSTRATUSZelnodes)
            ret.emplace_back(entry.second);

        return ret;
    }
    /** These above methods must be called by the GetFullZelnodeVector function. Do not call these methods by themselves */
};

/** Access to the ZN database (zelnodecache.dat)
 */
class ZelnodeDB
{
private:
    boost::filesystem::path pathZN;
    std::string strMagicMessage;

public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    ZelnodeDB();
    bool Write(const ZelnodeMan& zelnodemanToSave);
    ReadResult Read(ZelnodeMan& zelnodemanToLoad, bool fDryRun = false);
};


#endif //ZELCASHNODES_ZELNODEMAN_H
