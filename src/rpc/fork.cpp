// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "main.h"
#include "rpc/server.h"

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <univalue.h>
#include <boost/filesystem.hpp>

// TODO Fix import bug
#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))

using namespace std;

extern UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails);

typedef struct{
    uint256         hash;
    uint256         coinbase;
    int             num_tx;
    unsigned long   bits;
    int             file;
    unsigned int    data_pos;
    unsigned long   miner_time;
    double          network_time;
} SBlock;

typedef map<int, vector<SBlock*>> MBlocks;

MBlocks ReadBlockCSV(int minHeight, int maxHeight)
{
    // height -> struct
    MBlocks mapBlocks;

    // open block file
    boost::filesystem::path sBlockCSVPath = GetDataDir() / "blocks_v1.csv";
    FILE *fBlockCSVFile = fopen(sBlockCSVPath.string().c_str(), "r");
    if (fBlockCSVFile == NULL)
    {
        throw JSONRPCError(RPC_UNREACHABLE_FILE, "Cannot open file {datadir}/blocks_v1.csv");
    }

    // read file
    char sBlockCSVBuffer[256];
    while (fgets(sBlockCSVBuffer, sizeof(sBlockCSVBuffer), fBlockCSVFile) != NULL)
    {
        // skip header line
        // always starts with 'Height'
        if (sBlockCSVBuffer[0] == 'H')
        {
            continue;
        }
        int r_i = 0;    // read index
        int w_i = 0;    // write index

        // read height
        char sHeightBuffer[10];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != ',')
        {
            sHeightBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sHeightBuffer[w_i] = '\0';
        r_i ++;

        // check for height range
        int nHeight     = atoi(sHeightBuffer); 
        if ((minHeight > 0 && nHeight < minHeight)
                || (maxHeight > 0 && nHeight > maxHeight))
        {
            continue;
        }
        
        // read hash
        char sHashBuffer[128];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != ',')
        {
            sHashBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sHashBuffer[w_i] = '\0';
        r_i ++;

        // read coinbase
        char sCoinbaseBuffer[128];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != ',')
        {
            sCoinbaseBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sCoinbaseBuffer[w_i] = '\0';
        r_i ++;

        // read num tx
        char sNumTXBuffer[128];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != ',')
        {
            sNumTXBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sNumTXBuffer[w_i] = '\0';
        r_i ++;

        // read bits
        char sBitsBuffer[128];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != ',')
        {
            sBitsBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sBitsBuffer[w_i] = '\0';
        r_i ++;

        // read file
        char sFileBuffer[128];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != ',')
        {
            sFileBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sFileBuffer[w_i] = '\0';
        r_i ++;

        // read data pos
        char sDataPosBuffer[128];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != ',')
        {
            sDataPosBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sDataPosBuffer[w_i] = '\0';
        r_i ++;

        // read miner time
        char sMinerTimeBuffer[128];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != ',')
        {
            sMinerTimeBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sMinerTimeBuffer[w_i] = '\0';
        r_i ++;

        // read network time
        char sNetworkTimeBuffer[128];
        w_i = 0;
        while (sBlockCSVBuffer[r_i] != '\n')
        {
            sNetworkTimeBuffer[w_i++] = sBlockCSVBuffer[r_i++];
        }
        sNetworkTimeBuffer[w_i] = '\0';
        r_i ++;
        
        // alloc struct
        int nBlockIndex = mapBlocks[nHeight].size();
        mapBlocks[nHeight].push_back((SBlock*)calloc(1, sizeof(SBlock)));

        mapBlocks[nHeight][nBlockIndex]->hash.SetHex(sHashBuffer);
        mapBlocks[nHeight][nBlockIndex]->coinbase.SetHex(sCoinbaseBuffer);
        mapBlocks[nHeight][nBlockIndex]->num_tx         = atoi(sNumTXBuffer);
        mapBlocks[nHeight][nBlockIndex]->bits           = strtoul(sBitsBuffer, NULL, 10);
        mapBlocks[nHeight][nBlockIndex]->file           = atoi(sFileBuffer);
        mapBlocks[nHeight][nBlockIndex]->data_pos       = strtoul(sDataPosBuffer, NULL, 10);
        mapBlocks[nHeight][nBlockIndex]->miner_time     = (unsigned int)(atoi(sMinerTimeBuffer));
        mapBlocks[nHeight][nBlockIndex]->network_time   = atof(sNetworkTimeBuffer);
    }
    
    return mapBlocks;
}

void CleanBlockCSV(MBlocks mapBlocks)
{
    for (MBlocks::iterator it = mapBlocks.begin(); it != mapBlocks.end(); it++)
    {
        int nHeight             = it->first;
        vector<SBlock*> vBlocks = it->second;

        // clean pointers
        int nBlocks = vBlocks.size();
        for (int b_i = 0; b_i < nBlocks; b_i ++)
        {
            delete(vBlocks[b_i]);
        }
        vBlocks.clear();
    }

    mapBlocks.clear();
}

// hash -> time
typedef map<uint256, vector<double>> MInv;

MInv ReadInvCSV()
{
    MInv mapInv;

    // open block file
    boost::filesystem::path sInvCSVPath = GetDataDir() / "inv_v1.csv";
    FILE *fInvCSVFile = fopen(sInvCSVPath.string().c_str(), "r");
    if (fInvCSVFile == NULL)
    {
        throw JSONRPCError(RPC_UNREACHABLE_FILE, "Cannot open file {datadir}/blocks_v1.csv");
    }

    // read file
    char sInvCSVBuffer[256];
    while (fgets(sInvCSVBuffer, sizeof(sInvCSVBuffer), fInvCSVFile) != NULL)
    {
        // skip header line
        // always starts with 'Hash'
        if (sInvCSVBuffer[0] == 'H')
        {
            continue;
        }
        int r_i = 0;    // read index
        int w_i = 0;    // write index

        // read hash
        char sHashBuffer[128];
        w_i = 0;
        while (sInvCSVBuffer[r_i] != ',')
        {
            sHashBuffer[w_i++] = sInvCSVBuffer[r_i++];
        }
        sHashBuffer[w_i] = '\0';
        r_i ++;

        // read IP
        char sIPBuffer[128];
        w_i = 0;
        while (sInvCSVBuffer[r_i] != ',')
        {
            sIPBuffer[w_i++] = sInvCSVBuffer[r_i++];
        }
        sIPBuffer[w_i] = '\0';
        r_i ++;

        // read time
        char sTimeBuffer[128];
        w_i = 0;
        while (sInvCSVBuffer[r_i] != ',')
        {
            sTimeBuffer[w_i++] = sInvCSVBuffer[r_i++];
        }
        sTimeBuffer[w_i] = '\0';
        r_i ++;

        // Add entry
        uint256 hash(uint256S(string(sHashBuffer)));
        mapInv[hash].push_back(atof(sTimeBuffer));
    }

    return mapInv;
}

UniValue listforks(const UniValue& params, bool fHelp)
{
    // extract params
    int minHeight = -1;
    int maxHeight = -1;
    if (params.size() > 0)
    {
        minHeight = stoi(params[0].get_str());
    }
    if (params.size() > 1)
    {
        maxHeight = stoi(params[1].get_str());
    }
    
    MBlocks mapBlocks = ReadBlockCSV(minHeight, maxHeight);

    UniValue uForks(UniValue::VARR);

    // find forks
    for (MBlocks::iterator it = mapBlocks.begin(); it != mapBlocks.end(); it++)
    {
        int nHeight             = it->first;
        vector<SBlock*> vBlocks = it->second;
        int nBlocks             = vBlocks.size();
        
        if (nBlocks > 1)
        {
            // fork
            UniValue uFork(UniValue::VOBJ);
            uFork.push_back(Pair("height", nHeight));

            UniValue uBlocks(UniValue::VARR);
            for (int b_i = 0; b_i < nBlocks; b_i ++)
            {
                SBlock* sBlock = vBlocks[b_i];

                UniValue uBlock(UniValue::VOBJ);
                uBlock.push_back(Pair("hash",           sBlock->hash.GetHex()));
                uBlock.push_back(Pair("coinbase",       sBlock->coinbase.GetHex()));
                uBlock.push_back(Pair("num_tx",         sBlock->num_tx));
                uBlock.push_back(Pair("bits",           sBlock->bits));
                uBlock.push_back(Pair("minertime",      sBlock->miner_time));
                uBlock.push_back(Pair("networktime",    sBlock->network_time));

                uBlocks.push_back(uBlock);
            }

            uFork.push_back(Pair("blocks", uBlocks));

            uForks.push_back(uFork);
        }
    }

    CleanBlockCSV(mapBlocks);

    return uForks;
}

UniValue detectdoublespends(const UniValue& params, bool fHelp)
{
    // extract params
    int minHeight = -1;
    int maxHeight = -1;
    if (params.size() > 0)
    {
        minHeight = stoi(params[0].get_str());
    }
    if (params.size() > 1)
    {
        maxHeight = stoi(params[1].get_str());
    }
    
    UniValue uForks(UniValue::VARR);

    MBlocks mapBlocks = ReadBlockCSV(minHeight, maxHeight);

     // find forks
    for (MBlocks::iterator it = mapBlocks.begin(); it != mapBlocks.end(); it++)
    {
        int nHeight             = it->first;
        vector<SBlock*> vBlocks = it->second;
        int nBlocks             = vBlocks.size();
        
        if (nBlocks > 1)
        {
            // fork
            CBlock* cBlocks = (CBlock*)calloc(nBlocks, sizeof(CBlock));
            for (int b_i = 0; b_i < nBlocks; b_i ++)
            {
                SBlock* sBlock = vBlocks[b_i];

                // read block
                CBlockIndex*    pBlockIndex;
                bool            fBlockRead = true;
                if (mapBlockIndex.count(sBlock->hash) == 0)
                {
                    CDiskBlockPos cDiskBlockPos;
                    cDiskBlockPos.nFile     = sBlock->file;
                    cDiskBlockPos.nPos      = sBlock->data_pos;
                    if(!ReadBlockFromDisk(cBlocks[b_i], cDiskBlockPos, nHeight, Params().GetConsensus()))
                    {
                        fBlockRead = false;
                    }
                    else
                    {
                        if (cBlocks[b_i].GetHash() != sBlock->hash)
                        {
                            fBlockRead = false;
                        }
                    }
                }
                else
                {
                    pBlockIndex = mapBlockIndex[sBlock->hash];
                    if(!ReadBlockFromDisk(cBlocks[b_i], pBlockIndex, Params().GetConsensus()))
                    {
                        fBlockRead = false;
                    }
                }
            }

            UniValue uFork(UniValue::VOBJ);
            uFork.push_back(Pair("height", nHeight));

            // find uniq transactions
            UniValue uBlocks(UniValue::VARR);
            bool fDoubleSpend = false;
            for (int b_i = 0; b_i < nBlocks; b_i ++)
            {
                UniValue uBlock(UniValue::VOBJ);
                uBlock.push_back(Pair("hash",           vBlocks[b_i]->hash.GetHex()));
                uBlock.push_back(Pair("coinbase",       vBlocks[b_i]->coinbase.GetHex()));
                uBlock.push_back(Pair("num_tx",         vBlocks[b_i]->num_tx));
                uBlock.push_back(Pair("bits",           vBlocks[b_i]->bits));
                uBlock.push_back(Pair("minertime",      vBlocks[b_i]->miner_time));
                uBlock.push_back(Pair("networktime",    vBlocks[b_i]->network_time));

                
                UniValue uTXs(UniValue::VARR);
                int nTX_i = cBlocks[b_i].vtx.size();
                for (int t_i = 1; t_i < nTX_i; t_i ++)
                {
                    bool fUniqueTX = true;
                    for (int b_j = 0; b_j < nBlocks; b_j ++)
                    {
                        if (b_i == b_j)
                        {
                            continue;
                        }

                        int nTX_j = cBlocks[b_j].vtx.size();
                        for (int t_j = 1; t_j < nTX_j; t_j ++)
                        {
                            if (cBlocks[b_i].vtx[t_i].GetHash() == cBlocks[b_j].vtx[t_j].GetHash())
                            {
                                fUniqueTX = false;
                                continue;
                            }
                        }
                    }

                    if (fUniqueTX)
                    {
                        fDoubleSpend = true;

                        CTransaction tx = cBlocks[b_i].vtx[t_i];
                        uTXs.push_back(tx.GetHash().GetHex());
                    }
                }
                uBlock.push_back(Pair("uniquetx", uTXs));
                uBlocks.push_back(uBlock);
            }

            uFork.push_back(Pair("blocks", uBlocks));
            if (fDoubleSpend)
                uForks.push_back(uFork);
        }
    }

    CleanBlockCSV(mapBlocks);

    return uForks;
}

UniValue detectselfishmining(const UniValue& params, bool fHelp)
{
    // extract params
    int minHeight = -1;
    int maxHeight = -1;
    if (params.size() > 0)
    {
        minHeight = stoi(params[0].get_str());
    }
    if (params.size() > 1)
    {
        maxHeight = stoi(params[1].get_str());
    }
    
    MBlocks mapBlocks   = ReadBlockCSV(minHeight, maxHeight);
    
    UniValue uForks(UniValue::VARR);

    // find forks
    for (MBlocks::iterator it = mapBlocks.begin(); it != mapBlocks.end(); it++)
    {
        int nHeight             = it->first;
        vector<SBlock*> vBlocks = it->second;
        int nBlocks             = vBlocks.size();
        
        if (nBlocks > 1)
        {
            // Only list weird forks
            SBlock* sNextBlock      = mapBlocks[nHeight + 1][0];
            double nNextBlockTime   = 100000.0;
            for (int b_i = 0; b_i < nBlocks; b_i ++)
            {
                if (nNextBlockTime > (sNextBlock->network_time - vBlocks[b_i]->network_time))
                {
                    nNextBlockTime = sNextBlock->network_time - vBlocks[b_i]->network_time;
                }
            }

            // TODO get better metric
            if (nNextBlockTime > 10)
            {
                continue;
            }

            // fork
            UniValue uFork(UniValue::VOBJ);
            uFork.push_back(Pair("height",          nHeight));
            uFork.push_back(Pair("nextblocktime",   nNextBlockTime));

            UniValue uBlocks(UniValue::VARR);
            for (int b_i = 0; b_i < nBlocks; b_i ++)
            {
                SBlock* sBlock = vBlocks[b_i];

                UniValue uBlock(UniValue::VOBJ);
                uBlock.push_back(Pair("hash",           sBlock->hash.GetHex()));
                uBlock.push_back(Pair("coinbase",       sBlock->coinbase.GetHex()));
                uBlock.push_back(Pair("num_tx",         sBlock->num_tx));
                uBlock.push_back(Pair("bits",           sBlock->bits));
                uBlock.push_back(Pair("minertime",      sBlock->miner_time));
                uBlock.push_back(Pair("networktime",    sBlock->network_time));

                uBlocks.push_back(uBlock);
            }

            uFork.push_back(Pair("blocks", uBlocks));

            uForks.push_back(uFork);
        }
    }

    CleanBlockCSV(mapBlocks);

    return uForks;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "fork",               "listforks",              &listforks,              true         },
    { "fork",               "detectdoublespends",     &detectdoublespends,     true         },
    { "fork",               "detectselfishmining",    &detectselfishmining,    true         },
};

void RegisterForkRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
