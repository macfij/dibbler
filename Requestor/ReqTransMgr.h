/*
 * Dibbler - a portable DHCPv6
 *
 * authors: Tomasz Mrugalski <thomson@klub.com.pl>
 *
 * Released under GNU GPL v2 licence
 *
 * $Id: ReqTransMgr.h,v 1.2 2007-12-03 16:59:17 thomson Exp $
 */

#ifndef REQIFACEMGR_H
#define REQIFACEMGR_H

#include "IfaceMgr.h"
#include "ReqCfgMgr.h"

class ReqTransMgr {
public:
    ReqTransMgr(TIfaceMgr * ifaceMgr);
    void SetParams(ReqCfgMgr * cfgMgr);
    bool BindSockets();
    bool SendMsg();
    bool WaitForRsp();

private:
    void PrintRsp(char * buf, int bufLen);

    TIfaceMgr * IfaceMgr;
    SPtr<TIfaceIface> Iface;
    ReqCfgMgr * CfgMgr;
    SPtr<TIfaceSocket> Socket;
};

#endif
