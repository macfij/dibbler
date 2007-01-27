/*
 * Dibbler - a portable DHCPv6
 *
 * author:  Krzysztof Wnuk <keczi@poczta.onet.pl>
 * changes: Tomasz Mrugalski <thomson@klub.com.pl>
 *
 * released under GNU GPL v2 or later licence
 */

#include "AddrIA.h"
#include "ClntCfgPD.h"
#include "ClntOptIA_PD.h"
#include "ClntOptIAPrefix.h"
#include "ClntOptStatusCode.h"
#include "ClntIfaceMgr.h"
#include "Logger.h"
#include "Portable.h"
#include "DHCPConst.h"

/** 
 * Used in REQUEST, RENEW, REBIND, DECLINE and RELEASE
 * 
 * @param addrIA 
 * @param parent 
 */
TClntOptIA_PD::TClntOptIA_PD(SmartPtr<TAddrIA> addrPD, TMsg* parent)
    :TOptIA_PD(addrPD->getIAID(),addrPD->getT1(),addrPD->getT2(), parent)
{
    
    bool zeroTimes = false;
    if ( (parent->getType()==RELEASE_MSG) || (parent->getType()==DECLINE_MSG)) {
	this->T1 = 0;
	this->T2 = 0;
	zeroTimes = true;
    }

    DUID = 0;

    SmartPtr<TAddrPrefix> ptrPrefix;
    addrPD->firstPrefix();
    while ( ptrPrefix = addrPD->getPrefix() )
    {
	SubOptions.append(new TClntOptIAPrefix(ptrPrefix->get(), zeroTimes?0:ptrPrefix->getPref(), 
					       zeroTimes?0:ptrPrefix->getValid(), 
					       ptrPrefix->getLength(), this->Parent) );
    }
}

/** 
 * Used in SOLICIT constructor
 * 
 * @param ClntCfgPD 
 * @param parent 
 */
TClntOptIA_PD::TClntOptIA_PD(SmartPtr<TClntCfgPD> ClntCfgPD, TMsg* parent)
    :TOptIA_PD(1, ClntCfgPD->getT1(), ClntCfgPD->getT2(), parent)
{
    // FIXME: Copy all prefixes defined in CfgMgr (i.e. implement client hints)
}

/** 
 * Used to create object from received message
 * 
 * @param buf 
 * @param bufsize 
 * @param parent 
 */
TClntOptIA_PD::TClntOptIA_PD(char * buf,int bufsize, TMsg* parent)
:TOptIA_PD(buf,bufsize, parent)
{
    int pos=0;
    while(pos<bufsize) 
    {
        int code=buf[pos]*256+buf[pos+1];
        pos+=2;
        int length=buf[pos]*256+buf[pos+1];
	pos+=2;
        if ((code>0)&&(code<=26))
        {                
	    
	    if(allowOptInOpt(parent->getType(),OPTION_IA_PD,code))
            {
		SmartPtr<TOpt> opt= SmartPtr<TOpt>();
                switch (code)
                {
                case OPTION_IAPREFIX:
                    //  SmartPtr<TOptIAAddress> ptr;
		    SubOptions.append( new TClntOptIAPrefix(buf+pos,length,this->Parent));
                    break;
                case OPTION_STATUS_CODE:
                    SubOptions.append( new TClntOptStatusCode(buf+pos,length,this->Parent));
                    break;
                default:
		    Log(Warning) <<"Option opttype=" << code<< "not supported "
                        <<" in field of message (type="<< parent->getType() 
                        <<") in this version of server."<<LogEnd;
                    break;
                }
                if((opt)&&(opt->isValid()))
                    SubOptions.append(opt);
            }
            else
		Log(Warning) << "Illegal option received, opttype=" << code 
                << " in field options of IA_PD option"<<LogEnd;
        }
        else
        {
	    Log(Warning) <<"Unknown option in option IA_NA( optType=" 
			 << code << "). Option ignored." << LogEnd;
        };
        pos+=length;
    }
}

void TClntOptIA_PD::firstPrefix()
{
    SubOptions.first();
}

SmartPtr<TClntOptIAPrefix> TClntOptIA_PD::getPrefix()
{
    SmartPtr<TClntOptIAPrefix> ptr;
    do {
        ptr = (Ptr*) SubOptions.get();
        if (ptr)
            if (ptr->getOptType()==OPTION_IAPREFIX)
                return ptr;
    } while (ptr);
    return SmartPtr<TClntOptIAPrefix>();
}

int TClntOptIA_PD::countPrefix()
{
    SmartPtr< TOpt> ptr;
    SubOptions.first();
    int count = 0;
    while ( ptr = SubOptions.get() ) {
        if (ptr->getOptType() == OPTION_IAPREFIX) 
            count++;
    }
    return count;
}

int TClntOptIA_PD::getStatusCode()
{
    SmartPtr<TOpt> option;
    if (option=getOption(OPTION_STATUS_CODE))
    {
        SmartPtr<TClntOptStatusCode> statOpt=(Ptr*) option;
        return statOpt->getCode();
    }
    return STATUSCODE_SUCCESS;
}

void TClntOptIA_PD::setContext(SmartPtr<TClntIfaceMgr> ifaceMgr, 
                               SmartPtr<TClntTransMgr> transMgr, 
                               SmartPtr<TClntCfgMgr> cfgMgr, 
                               SmartPtr<TClntAddrMgr> addrMgr,
                               SmartPtr<TDUID> srvDuid, SmartPtr<TIPv6Addr> srvAddr, 
			       TMsg * originalMsg)
{
    this->AddrMgr=addrMgr;
    this->IfaceMgr=ifaceMgr;
    this->TransMgr=transMgr;
    this->CfgMgr=cfgMgr;
    this->DUID=srvDuid;
    if (srvAddr) {
        this->Unicast = true;
    } else {
        this->Unicast = false;
    }
    this->Prefix=srvAddr;
    this->OriginalMsg = originalMsg;
    this->Iface = Parent->getIface();
}

TClntOptIA_PD::~TClntOptIA_PD()
{

}

bool TClntOptIA_PD::doDuties()
{
    if (!OriginalMsg) {
	Log(Error) << "Internal error. Unable to set prefixes: setContext() not called." << LogEnd;
	return false;
    }
	
    switch(OriginalMsg->getType()) {
    case REQUEST_MSG:
    case SOLICIT_MSG:
	return addPrefixes();
    case RELEASE_MSG:
	return delPrefixes();
    case RENEW_MSG:
	return updatePrefixes();
    default:
	break;
    }
    
    return true;
} 

SmartPtr<TClntOptIAPrefix> TClntOptIA_PD::getPrefix(SmartPtr<TIPv6Addr> prefix)
{
   SmartPtr<TClntOptIAPrefix> optPrefix;
    this->firstPrefix();
    while(optPrefix=this->getPrefix())
    {   
        //!memcmp(optAddr->getAddr(),addr,16)
        if ((*prefix)==(*optPrefix->getPrefix()))
            return optPrefix;
    }
    return 0;
}

bool TClntOptIA_PD::addPrefixes()
{
    SmartPtr<TClntOptIAPrefix> prefix;
    this->firstPrefix();
    
    while (prefix = this->getPrefix()) {
	AddrMgr->addPrefix(this->DUID, this->Prefix, this->Iface, this->IAID, this->T1, this->T2,
			   prefix->getPrefix(), prefix->getPref(), prefix->getValid(), prefix->getPrefixLength(), false);
	
	if (!IfaceMgr->addPrefix(this->Iface, prefix->getPrefix(), prefix->getPrefixLength(), prefix->getPref(), prefix->getValid())) {
	    string tmp = error_message();
	    Log(Error) << "Prefix error encountered during add operation: " << tmp << LogEnd;
	    setState(STATE_FAILED);
	    return false;
	}
    }

    setState(STATE_CONFIGURED);
    return true;
}

bool TClntOptIA_PD::delPrefixes()
{
    SmartPtr<TClntOptIAPrefix> prefix;
    this->firstPrefix();
    SmartPtr<TClntCfgIface> cfgIface = this->CfgMgr->getIface(this->Iface);

    while (prefix = this->getPrefix()) {
	AddrMgr->delPrefix(CfgMgr->getDUID(), this->IAID, prefix->getPrefix(), false);
	
	if (!IfaceMgr->delPrefix(this->Iface, prefix->getPrefix(), prefix->getPrefixLength() )) {
	    string tmp = error_message();
	    Log(Error) << "Prefix error encountered during delete operation: " << tmp << LogEnd;
	    setState(STATE_FAILED);
	    return true;
	}
    }

    setState(STATE_DISABLED);
    return true;
}

bool TClntOptIA_PD::updatePrefixes()
{
    SmartPtr<TClntOptIAPrefix> prefix;
    this->firstPrefix();

    SPtr<TAddrIA> pd = AddrMgr->getPD(getIAID());
    if (!pd) {
	Log(Error) << "Unable to find PD (iaid=" << getIAID() << "). PD renewal failed." << LogEnd;
	return false;
    }

    // FIXME: Implement AddrMgr/IfaceMgr PD update
    Log(Error) << "#### Implement AddrMgr/IfaceMgr PD update" << LogEnd;

    setState(STATE_CONFIGURED);
    return true;
}

void TClntOptIA_PD::setIface(int iface) {
    this->Iface    = iface;
}


bool TClntOptIA_PD::isValid()
{
    SmartPtr<TClntOptIAPrefix> prefix;
    this->firstPrefix();
    while (prefix = this->getPrefix()) {
	if (prefix->getPrefix()->linkLocal()) {
	    Log(Warning) << "Address " << prefix->getPrefix()->getPlain() << " used in IA (IAID=" 
			 << this->IAID << ") is link local. The whole IA option is considered invalid."
			 << LogEnd;
	    return false;
	}
    }

    if (Valid)
        return this->T1<=this->T2;
    else
        return false;
}

void TClntOptIA_PD::setState(EState state)
{
    SmartPtr<TClntCfgIface> cfgIface = CfgMgr->getIface(this->Iface);

    SPtr<TClntCfgPD> cfgPD = cfgIface->getPD(getIAID());
    if (!cfgPD) {
	Log(Error) << "Unable to find PD with iaid=" << getIAID() << " on the "
		   << cfgIface->getFullName() << " interface (CfgMgr)." << LogEnd;
	return;
    }
    cfgPD->setState(STATE_CONFIGURED);

    SPtr<TAddrIA> addrPD = AddrMgr->getPD(getIAID());
    if (!addrPD) {
	/* Log(Error) << "Unable to find PD with iaid=" << getIAID() << " on the "
	   << cfgIface->getFullName() << " interface (AddrMgr)." << LogEnd; */
	/* Don't complain about it. It is normal that IA is being deleted when there
	   are no more prefixes in it */
	return;
    }
    addrPD->setState(STATE_CONFIGURED);
}
